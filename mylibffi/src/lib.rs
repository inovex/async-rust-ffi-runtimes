// Nearly everything is unsafe because of FFI
#![allow(clippy::missing_safety_doc)]

use core::{ffi::c_char, slice};
use std::{error::Error, ffi::CString};

use async_ffi::{FfiFuture, FutureExt};
use async_trait::async_trait;

use mylib::*;

#[repr(C)]
pub struct FfiDataHolder {
    ptr: *const u8,
    len: usize,
    drop: unsafe extern "C" fn(*const FfiDataHolder),
    _pin: core::marker::PhantomPinned,
}

// Extra layer of indirection, should be replaced by proper use of magic
struct DataWrapper {
    data_holder: *const FfiDataHolder,
}

impl DataHolder for DataWrapper {
    fn bytes(&self) -> &[u8] {
        eprintln!("DataWrapper::byte");
        unsafe {
            let ptr = (*(self.data_holder)).ptr;
            let len = (*(self.data_holder)).len;
            eprintln!("DataWrapper::bytes: ptr={:?}, len={}", ptr, len);
            slice::from_raw_parts(ptr, len)
        }
    }
}

impl Drop for DataWrapper {
    fn drop(&mut self) {
        eprintln!("DataWrapper::drop");
        unsafe {
            ((*(self.data_holder)).drop)(self.data_holder);
        }
    }
}

pub struct FfiDataAccess;

#[repr(C)]
pub struct FfiDataAccessVTable {
    /// The returned data is exclusively owned by the caller.
    get_data:
        unsafe extern "C" fn(*mut FfiDataAccess, *const c_char) -> FfiFuture<*mut FfiDataHolder>,
    drop: unsafe extern "C" fn(*mut FfiDataAccess),
}

struct DataAccessWrapper {
    data: *mut FfiDataAccess,
    vtable: *const FfiDataAccessVTable,
}

unsafe impl Send for DataAccessWrapper {}
unsafe impl Sync for DataAccessWrapper {}

impl Drop for DataAccessWrapper {
    fn drop(&mut self) {
        eprintln!("DataAccessWrapper::drop");
        unsafe {
            ((*(self.vtable)).drop)(self.data);
        }
    }
}

#[async_trait]
impl DataAccess for DataAccessWrapper {
    async fn get_data(&self, key: &str) -> Result<Box<dyn DataHolder>, Box<dyn Error>> {
        eprintln!("DataAccessWrapper::get_data");
        let s = CString::new(key).expect("CString::new failed");
        // pin the C string, to prevent the compiler from moving the data
        let pin = Box::into_pin(s.into_boxed_c_str());
        unsafe {
            let sp = pin.as_ptr();
            let data_holder = ((*(self.vtable)).get_data)(self.data, sp).await;
            Ok(Box::new(DataWrapper { data_holder }))
        }
    }
}

pub struct FfiLib {
    instance: Lib<DataAccessWrapper>,
    _pin: core::marker::PhantomPinned,
}

#[no_mangle]
pub unsafe extern "C" fn mylib_alloc(
    data_access: *mut FfiDataAccess,
    data_access_vtable: *const FfiDataAccessVTable,
) -> *mut FfiLib {
    eprintln!("mylib_alloc");
    let data_access_wrapper = DataAccessWrapper {
        data: data_access,
        vtable: data_access_vtable,
    };
    let lib = Box::new(FfiLib {
        instance: Lib::new(data_access_wrapper),
        _pin: core::marker::PhantomPinned {},
    });
    Box::into_raw(lib)
}

#[no_mangle]
pub unsafe extern "C" fn mylib_should_run(ffi_lib: *mut FfiLib, postcode: u32) -> FfiFuture<bool> {
    eprintln!("mylib_should_run");
    let lib = &(*ffi_lib).instance;
    // TODO: return proper error
    let postcode = Postcode::new(postcode).unwrap();
    async {
        match lib.should_run(postcode).await {
            Ok(value) => value,
            Err(e) => panic!("error from mylib: {}", e),
        }
    }
    .into_ffi()
}

#[no_mangle]
pub unsafe extern "C" fn mylib_free(lib: *mut FfiLib) {
    eprintln!("mylib_free");
    drop(Box::from_raw(lib));
}
