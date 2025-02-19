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
					/***** base cases *****/
					// 2 values
			        (VAL(v1), VAL(v2)) => VAL(op.compute_val(v1, v2)),

			        // values cannot (yet) combine with variables
			        // eventually, things like 0*x and 1^x will simplify
			        (VAR, VAL(v)) => HOLD(eb!(VAR, op, VAL(v))),
                    (VAL(v), VAR) => HOLD(eb!(VAL(v), op, VAR)),
			        // (VAL(v), ast) => op.apply_left_num(v, ast.reduce()?),
			        // (ast, VAL(v)) => op.apply_right_num(ast.reduce()?, v),
			        
			        // variables cannot combine with each other (yet) either
			        (VAR, VAR) => HOLD(eb!(VAR, op, VAR)),

			        // block infinite recursion by preserving Hold on already-reduced expressions
			        (HOLD(r_ex), ex) => HOLD(eb!(HOLD(r_ex), op, ex.reduce())),
			        (ex, HOLD(r_ex)) => HOLD(eb!(ex.reduce(), op, HOLD(r_ex))),

			        /***** recursive cases *****/
			        // both sides reducible
			        (ex1, ex2) => APPLY(eb!(ex1.reduce(), op, ex2.reduce())),
    			}.reduce() // <-- IMPORTANT! without this we miss the final simplification
			},
			_ => panic!("WTF")
		}
	}
}

#[derive(Display, Clone, Copy)]
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
	pub fn compute_val(&self, left: NumType, right: NumType) -> NumType {
		use BinOp::*;
		match self {
			PLUS => left + right,
			MINUS => left - right,
			TIMES => left * right,
			DIV => left / right,
			POW => left ^ (right),
		}
	}

	// NOTE: convert PLUS and TIMES to canonicalize the un-simplified operand on the right 
	//  (or left? -- see which works better later)!
	// also, would it be better to just overload the arithmetic operators for AST node,
	// then apply those correspondingly? possibly...
	// not a big fan of this method, and it doesn't *fully* work (currently) anyway
	pub fn apply_left_num(&self, left: NumType, right: ASTNode) -> ASTNode {
		use ASTNode::*;
		use BinOp::*;
		use NumType::*;
		
		match self {
			PLUS => match left {
				I(0) | F(0.0) => right,
				_ => HOLD(eb!(VAL(left), PLUS, right)),
			},
			MINUS => match left {
				I(0) | F(0.0) => APPLY(eb!(VAL(I(-1)), TIMES, right)),
				_ => HOLD(eb!(VAL(left), *self, right)),
			},
			TIMES => match left {
				I(0) | F(0.) => VAL(I(0)),
				I(1) | F(1.) => right,
				_ => HOLD(eb!(VAL(left), *self, right)),
			},
			DIV => match left {
				I(0) | F(0.) => panic!("divide by zero error: attempted to perform {right} / 0"),
				I(1) | F(1.) => right,
				_ => HOLD(eb!(VAL(left), *self, right)),
			},
			POW => match left {
				I(0) | F(0.) => VAL(I(1)),
				I(1) | F(1.) => right,
				_ => HOLD(eb!(VAL(left), *self, right)),
			}
		}
	}

	pub fn apply_right_num(&self, left: ASTNode, right: NumType) -> ASTNode {
		use ASTNode::*;
		use BinOp::*;
		use NumType::*;
		
		match self {
			PLUS => match right {
				I(0) | F(0.0) => left,
				_ => HOLD(eb!(VAL(right), PLUS, left)),
			},
			MINUS => match right {
				I(0) | F(0.0) => APPLY(eb!(VAL(I(-1)), TIMES, left)),
				_ => HOLD(eb!(left, *self, VAL(right))),
			},
			TIMES => match right {
				I(0) | F(0.) => VAL(I(0)),
				I(1) | F(1.) => left,
				_ => HOLD(eb!(VAL(right), *self, left)),
			},
			DIV => match right {
				I(0) | F(0.) => panic!("divide by zero error: attempted to perform {left} / 0"),
				I(1) | F(1.) => left,
				_ => HOLD(eb!(left, *self, VAL(right))),
			},
			POW => match right {
				I(0) | F(0.) => VAL(I(1)),
				I(1) | F(1.) => left,
				_ => HOLD(eb!(left, *self, VAL(right))),
			}
		}
	}
}
