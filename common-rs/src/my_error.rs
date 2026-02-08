use std::error::Error;
use std::fmt::{Display, Formatter};

#[derive(Debug)]
pub struct MyError {
    pub msg: String,
}

impl Display for MyError {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.msg)
    }
}

impl Error for MyError {}

impl MyError {
    pub fn new(msg: String) -> Self {
        MyError { msg }
    }
}
