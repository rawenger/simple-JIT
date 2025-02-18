use core::ops::{Add, Sub, Mul, Div, BitXor};
use derive_more::Display;

#[derive(Copy, Clone, Display, Debug, PartialEq, PartialOrd)]
pub enum NumType {
	// #[display("{_0}")]
	F(f64),
	I(i64),
}

impl From<i64> for NumType {
    fn from(value: i64) -> Self {
        Self::I(value)
    }
}

impl From<f64> for NumType {
    fn from(value: f64) -> Self {
        Self::F(value)
    }
}

impl std::str::FromStr for NumType {
    type Err = anyhow::Error;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        if s.contains('.') {
        	Ok(Self::F(s.parse::<f64>()?))
        } else {
        	Ok(Self::I(s.parse::<i64>()?))
        }
    }
}

impl core::cmp::Eq for NumType {

}

impl Add for NumType {
    type Output = Self;

    fn add(self, rhs: Self) -> Self::Output {
        use NumType::{I,F};
        match (self, rhs) {
        	(I(l), I(r)) => I(l + r),
            (F(l), F(r)) => F(l + r),
            (F(l), I(r)) => F(l + (r as f64)),
            (I(l), F(r)) => F((l as f64) + r),
        }
    }
}

impl Sub for NumType {
    type Output = Self;

    fn sub(self, rhs: Self) -> Self::Output {
        use NumType::{I,F};
        match (self, rhs) {
        	(I(l), I(r)) => I(l - r),
            (F(l), F(r)) => F(l - r),
            (F(l), I(r)) => F(l - (r as f64)),
            (I(l), F(r)) => F((l as f64) - r),
        }
    }
}

impl Mul for NumType {
    type Output = Self;

    fn mul(self, rhs: Self) -> Self::Output {
        use NumType::{I,F};
        match (self, rhs) {
        	(I(l), I(r)) => I(l * r),
            (F(l), F(r)) => F(l * r),
            (F(l), I(r)) => F(l * (r as f64)),
            (I(l), F(r)) => F((l as f64) * r),
        }
    }
}

impl Div for NumType {
    type Output = Self;

    fn div(self, rhs: Self) -> Self::Output {
        use NumType::{I,F};
        match (self, rhs) {
        	(I(l), I(r)) if l % r == 0 => I(l / r),
        	(I(l), I(r)) => F((l as f64) / (r as f64)),
            (F(l), F(r)) => F(l / r),
            (F(l), I(r)) => F(l / (r as f64)),
            (I(l), F(r)) => F((l as f64) * r),
        }
    }
}

impl BitXor for NumType {
    type Output = Self;

    fn bitxor(self, rhs: Self) -> Self::Output {
        use NumType::{I,F};
        match (self, rhs) {
        	(I(l), I(r)) if r >= 0 => I(l.pow(r as u32)),
        	(I(l), I(r)) => F((l as f64).powf(r as f64)),
            (F(l), F(r)) => F(l.powf(r)),
            (F(l), I(r)) => F(l.powi(r as i32)),
            (I(l), F(r)) => F((l as f64).powf(r)),
        }
    }
}