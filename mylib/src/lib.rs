use std::{
    error::Error,
    fmt::Display,
};

use async_trait::async_trait;
use chrono::{DateTime, Utc};
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

pub struct Lib<D: DataAccess> {
    data_access: D,
}

#[derive(Debug)]
pub enum MyError {
    InvalidData,
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

impl<D: DataAccess> Lib<D> {
    pub fn new(data_access: D) -> Lib<D> {
        Lib {
            data_access,
        }
    }

    pub async fn should_run(&self, _when: DateTime<Utc>) -> Result<bool, Box<dyn Error>> {
        let k = format!("https://api.stromgedacht.de/v1/now?zip={}", 76137);
        let resp = self.data_access.get_data(&k).await?;
        let data = &resp.bytes();
        if data.len() == 0 {
            return Err(Box::new(MyError::InvalidData));
        }
        let cur: CurrentState = serde_json::from_slice(data)?;
        Ok(cur.state <= 1)
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[derive(Default)]
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
        let mut data_access = MockDataAccess::default();
        data_access.state = 1;
        let lib = Lib::new(data_access);
        assert!(lib.should_run(Utc::now()).await?);
        Ok(())
    }
}
