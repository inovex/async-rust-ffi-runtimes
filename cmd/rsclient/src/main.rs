use std::error::Error;

use async_trait::async_trait;

use mylib::*;

struct MyDataHolder {
    data: Vec<u8>,
}

impl DataHolder for MyDataHolder {
    fn bytes(&self) -> &[u8] {
        self.data.as_ref()
    }
}

struct MyTransport {}

#[async_trait]
impl DataAccess for MyTransport {
    async fn get_data(&self, key: &str) -> Result<Box<dyn DataHolder>, Box<dyn Error>> {
        let mut res = surf::get(key).await?;
        let data = res.body_bytes().await?;
        Ok(Box::new(MyDataHolder{data}))
    }
}

fn main() {
    let transport = MyTransport {};
    let lib = Lib::new(transport);

    if let Err(err) = smol::block_on(async {
        let ok = lib.should_run(Postcode::new(76137)?).await?;
        if ok {
            println!("ok");
        }
        Ok::<(), Box<dyn Error>>(())
    }) {
        eprintln!("error: {err}");
    }
}
