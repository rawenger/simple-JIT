/*
 * ast.rs -- abstract syntax tree representation of equation
 */

#![allow(unused)]
use anyhow::{anyhow, bail, Result};
use derive_more::Display;
use crate::recurrence;
use crate::numtype::NumType;
use std::cell::Cell;

/// convenience shorthand to construct `Ex` object and place it in a Box
/// (name short for "Ex inside Box")
macro_rules! eb {
	($left:expr, $op:expr, $right:expr) => { 
		Box::new(Ex($left, $op, $right))
	};
}

// note: Box may need to hold MaybeUnint<ASTNode>, we'll see.
type Operand = ASTNode;


#[derive(Display)]
#[display("{}", root)]
pub struct AST {
	root: ASTNode,
}

impl AST {

	pub fn construct(pfix: &Vec<recurrence::Token>) -> Option<Self> {
		/* CONSTRUCTION ALGORITHM:
		 * 
		 * ** construct AST from postfix **
		 *	- construct left child, pass into recursion, propagate up until root
		 */

		let mut calc = Vec::new();

		for tok in pfix {
			use recurrence::Token;
			use ASTNode::*;
			match tok {
				Token::NUM(n) => calc.push(VAL(*n)),
				Token::ALPHA(_) => calc.push(VAR),
				tok => {
					let (n2, n1) = (calc.pop()?, calc.pop()?);
					let compute = match tok {
						Token::PLUS => APPLY(eb!(n1, BinOp::PLUS, n2)),
						Token::MINUS => APPLY(eb!(n1, BinOp::MINUS, n2)),
						Token::TIMES => APPLY(eb!(n1, BinOp::TIMES, n2)),
						Token::DIV => APPLY(eb!(n1, BinOp::DIV, n2)),
						Token::POW => APPLY(eb!(n1, BinOp::POW, n2)),
						_ => return None,
					};
					calc.push(compute);
				}
			}
		}

		// expression *should* already be valid
		return Some(Self {
			root: calc.pop()?
		})
	}


	/// performs algebraic simplification on the tree
	// TODO: how can I make this able to take `&self`? 
	//  what types do I need to wrap `self.root` in??
	pub fn reduce(self) -> Self {
		Self {
			root: self.root.reduce()
		}
	}

}

#[derive(Display)]
#[display("{_0} {_1} {_2}")]
struct Ex(Operand, BinOp, Operand);

#[derive(Display)]
enum ASTNode {
	#[display("({_0})")]
	APPLY(Box<Ex>),

	#[display("({_0})")]
	HOLD(Box<Ex>),

	#[display("{_0}")]
	VAL(NumType),

	#[display("x")]
	VAR,
}
		
impl ASTNode {
	/// reduces itself (if possible) by
	/// converting ::OP to ::VAL
	fn reduce(self) -> Self {
		use ASTNode::*;
		match self {
			VAR => VAR,
			VAL(v) => VAL(v),
			HOLD(re) => HOLD(re),
			APPLY(box Ex(left, op, right)) => {
				match (left, right) {
					// 2 values
			        (VAL(v1), VAL(v2)) => VAL(op.compute(v1, v2)),

			        // values cannot (yet) combine with variables
			        // eventually, things like 0*x and 1^x will simplify
			        (VAR, VAL(v)) => HOLD(eb!(VAR, op, VAL(v))),
			        (VAL(v), VAR) => HOLD(eb!(VAL(v), op, VAR)),

			        // variables cannot combine with each other (yet) either
			        (VAR, VAR) => HOLD(eb!(VAR, op, VAR)),

			        // block infinite recursion by preserving Hold on already-reduced expressions
			        (HOLD(r_ex), ex) => HOLD(eb!(HOLD(r_ex), op, ex.reduce())),
			        (ex, HOLD(r_ex)) => HOLD(eb!(ex.reduce(), op, HOLD(r_ex))),

			        // both sides reducible
			        (ex1, ex2) => APPLY(eb!(ex1.reduce(), op, ex2.reduce())),
    			}.reduce() // <-- IMPORTANT! without this we miss the final simplification
			},
			_ => panic!("WTF")
		}
	}
}

#[derive(Display)]
enum BinOp {
	#[display("+")]
	PLUS,
	#[display("-")]
	MINUS,
	#[display("*")]
	TIMES,
	#[display("/")]
	DIV,
	#[display("^")]
	POW, // .0 is base, .1 is exp
}

impl BinOp {
	pub fn compute(&self, left: NumType, right: NumType) -> NumType {
		use BinOp::*;
		match self {
			PLUS => left + right,
			MINUS => left - right,
			TIMES => left * right,
			DIV => left / right,
			POW => left ^ (right),
		}
	}
}
