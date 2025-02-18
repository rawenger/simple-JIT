#![feature(box_patterns, try_blocks)]
mod ast;
mod recurrence;
mod numtype;

use clap::Parser;
use anyhow::Result;
use numtype::NumType;
use recurrence::Recurrence;


#[derive(Parser, Debug)]
#[command(name = "simple-jit", author = "Ryan Wenger", version, about, long_about = None)]
struct Args {
    /// recurrence equation (in variable `x`) to compute
    #[arg(default_value_t = String::from("((54 + x) / (3 * x)) - (4 * 2)"))]
    pub equation: String,

    /// compile equation into machine code before computation (supported on aarch64 and x86_64)
    #[arg(long, default_value_t = true)]
    pub use_jit: bool,

    /// Number of iterations
    #[arg(short, long, default_value_t = 100000)]
    pub num_iter: usize,

    /// initial value of `x`
    #[arg(short='0', long, default_value_t = String::from("0"))]
    pub x0: String,
}

fn main() -> Result<()> {
    let args = Args::parse();
    // dbg!(&args);
    let r = Recurrence::new(&args.equation)?;
    let x0 = args.x0.parse::<NumType>()?;
    println!("computing in-place: {}", r.compute(x0, args.num_iter));
    let mut a = r.make_ast()?;
    println!("AST: {a}");
    a = a.reduce();
    println!("after reducing: {a}");
    // println!("result: {}", r.compute(args.x0, args.num_iter));
    Ok(())
}
