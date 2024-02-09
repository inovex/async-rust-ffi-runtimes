#include "mylib.hpp"

#include <exception>

namespace {

struct FfiDataAccessVTable {
    ::FfiFuture/* <FfiDataHolder*> */ (*get_data)(void*, char const*);
    void (*drop)(void*);
};

extern "C" {

::FfiLib* mylib_alloc(void* data_access, ::FfiDataAccessVTable* data_access_vtable);
::FfiFuture/* <bool> */ mylib_should_run(::FfiLib* mylib, std::uint32_t postcode);
void mylib_free(::FfiLib* mylib);

} // extern "C"
} // namespace

namespace mylib {
namespace {

struct DataAccessWrapper {
    DataAccessWrapper(std::unique_ptr<DataAccess> data_access)
        : vtable{
            .get_data = &DataAccessWrapper::get_data,
            .drop = &DataAccessWrapper::drop,
        }
        , wrapped{std::move(data_access)} {}

    static ::FfiFuture get_data(void* self, char const* key) {
        return static_cast<DataAccessWrapper*>(self)->wrapped->get_data();
    }

    static void drop(void* self) {
        auto* p = static_cast<DataAccessWrapper*>(self);
        delete p;
    }

    ::FfiDataAccessVTable vtable;
    std::unique_ptr<DataAccess> wrapped;
};

} // namespace

DataHolderBase::~DataHolderBase() = default;
DataAccess::~DataAccess() = default;

Lib::Lib(std::unique_ptr<DataAccess> data_access) : m_mylib{nullptr} {
    // this uses new because the memory management is taken over by the Rust library anyway
    auto* wrapper = new DataAccessWrapper{std::move(data_access)};
    auto* instance = ::mylib_alloc(wrapper, &wrapper->vtable);
    if (!instance) {
        throw std::runtime_error{"mylib_alloc returned null"};
    }
    m_mylib = instance;
}

Lib::~Lib() {
    if (m_mylib) {
        ::mylib_free(m_mylib);
    }
}

::FfiFuture Lib::should_run(std::uint32_t postcode) {
    return ::mylib_should_run(m_mylib, postcode);
}

} // namespace mylib
