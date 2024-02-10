use std::{error::Error, fmt::Display};

use async_trait::async_trait;
use serde::Deserialize;

// Trait to hold data without copying and being able to free it with the correct allocator.
pub trait DataHolder {
    fn bytes(&self) -> &[u8];
}

#[cfg_attr(target_arch = "wasm32", async_trait(?Send))]
#[cfg_attr(not(target_arch = "wasm32"), async_trait)]
pub trait DataAccess {
    async fn get_data(&self, key: &str) -> Result<Box<dyn DataHolder>, Box<dyn Error>>;
}

#[derive(Debug)]
pub enum MyError {
    InvalidData,
    InvalidPostcode,
}

impl Error for MyError {}
impl Display for MyError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let s = format!("{:?}", self);
        write!(f, "{s}")
    }
}

#[derive(Deserialize)]
struct CurrentState {
    state: i8,
}

pub struct Postcode {
    code: u32,
}

impl Postcode {
    #[allow(clippy::manual_range_contains)]
    pub fn new(code: u32) -> Result<Self, MyError> {
        // TODO: probably a separate error type would be better?
        if code > 99999 || code < 10000 {
            return Err(MyError::InvalidPostcode);
        }
        Ok(Postcode { code })
    }
}

pub struct Lib<D: DataAccess> {
    data_access: D,
}

impl<D: DataAccess> Lib<D> {
    pub fn new(data_access: D) -> Lib<D> {
        Lib { data_access }
    }

    pub async fn should_run(&self, postcode: Postcode) -> Result<bool, Box<dyn Error>> {
        let k = format!("https://api.stromgedacht.de/v1/now?zip={}", postcode.code);
        let resp = self.data_access.get_data(&k).await?;
        let data = &resp.bytes();
        if data.is_empty() {
            return Err(Box::new(MyError::InvalidData));
        }
        let cur: CurrentState = serde_json::from_slice(data)?;
        Ok(cur.state <= 1)
    }
}

#[cfg(test)]
mod test {
    use super::*;

    struct MockDataAccess {
        state: i8,
    }

    impl DataHolder for Vec<u8> {
        fn bytes(&self) -> &[u8] {
            self.as_ref()
        }
    }

    #[async_trait]
    impl DataAccess for MockDataAccess {
        async fn get_data(&self, _key: &str) -> Result<Box<dyn DataHolder>, Box<dyn Error>> {
            let s = format!(r#"{{"state":{}}}"#, self.state);
            Ok(Box::new(Vec::from(s.as_bytes())))
        }
    }

    #[futures_test::test]
    async fn test_should_run() -> Result<(), Box<dyn Error>> {
        let data_access = MockDataAccess { state: 1 };
        let lib = Lib::new(data_access);
        assert!(lib.should_run(Postcode::new(76137).unwrap()).await?);
        Ok(())
    }
}
