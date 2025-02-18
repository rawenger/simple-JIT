/*
 * ast.rs -- abstract syntax tree representation of equation
 */

#![allow(unused)]
use anyhow::{bail, Result};
use derive_more::Display;
use crate::recurrence::{self,NumType};
use std::cell::Cell;

// note: Box may need to hold MaybeUnint<ASTNode>, we'll see.
type Operand = Box<ASTNode>;

// internal representation for numbers

#[derive(Display)]
#[display("{}", root)]
pub struct AST {
	root: ASTNode,
}

impl AST {

	pub fn construct(pfix: &Vec<recurrence::Token>) -> Result<Self> {
		/* CONSTRUCTION ALGORITHM:
		 * 
		 * ** construct AST from postfix **
		 *	- construct left child, pass into recursion, propagate up until root
		 */


		// let test1 = ASTNode::OP(Op::PLUS(
		// 	Box::from(ASTNode::OP(Op::TIMES(
		// 		Box::from(ASTNode::VAL(1.5)), Box::from(ASTNode::VAR))))
		// 	,
		// 	Box::from(ASTNode::OP(Op::DIV(
		// 		Box::from(ASTNode::VAL(3.0)), 
		// 		Box::from(ASTNode::OP(Op::MINUS(
		// 			Box::from(ASTNode::VAL(6.0)), 
		// 			Box::from(ASTNode::VAR))))
		// 		)))
		// 	));
		// println!("test1: {test1}");

		let mut calc = Vec::new();

		for tok in pfix {
			use recurrence::Token;
			use ASTNode::*;
			match tok {
				Token::NUM(n) => calc.push(Operand::new(VAL(*n))),
				Token::ALPHA(_) => calc.push(Operand::new(VAR)),
				tok => {
					let n2 = calc.pop().unwrap();
					let n1 = calc.pop().unwrap();
					let compute = match tok {
						Token::PLUS => OP(n1, Op::PLUS, n2),
						Token::MINUS => OP(n1, Op::MINUS, n2),
						Token::TIMES => OP(n1, Op::TIMES, n2),
						Token::DIV => OP(n1, Op::DIV, n2),
						Token::POW => OP(n1, Op::POW, n2),
						_ => panic!("unknown error occurred")
					};
					calc.push(opn(compute));
				}
			}
		}

		return Ok(Self {
			root: *calc.pop().unwrap()
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
enum ASTNode {
	#[display("({_0} {_1} {_2})")]
	OP(Operand, Op, Operand),

	#[display("({_0} {_1} {_2})")]
	ReducedOp(Operand, Op, Operand),

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
			ReducedOp(l, o, r) => ReducedOp(l, o, r),
			OP(box left, op, box right) => {
				match (left, right) { // this is going to get very repetitive...
					// 2 values
			        (VAL(v1), VAL(v2)) => VAL(op.compute(v1, v2)),

			        (VAR, VAL(v)) => ReducedOp(opn(VAR), op, opn(VAL(v))),
			        (VAR, VAL(v)) => ReducedOp(opn(VAL(v)), op, opn(VAR)),
			        (VAR, VAR) => ReducedOp(opn(VAR), op, opn(VAR)),

			        (ReducedOp(l, o, r), o2) => ReducedOp(opn(ReducedOp(l, o, r)), op, opn(o2.reduce())),
			        (o2, ReducedOp(l, o, r)) => ReducedOp(opn(o2.reduce()), op, opn(ReducedOp(l, o, r))),

			        // both OP
			        (o1, o2) => OP(opn(o1.reduce()), op, opn(o2.reduce())),
    			}.reduce()
			},
			_ => panic!("WTF")
		}
	}
}

/// shorthand
fn opn(n: ASTNode) -> Operand {
	Operand::new(n)
}

#[derive(Display)]
enum Op {
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

impl Op {
	pub fn compute(&self, left: NumType, right: NumType) -> NumType {
		use Op::*;
		match self {
			PLUS => left + right,
			MINUS => left - right,
			TIMES => left * right,
			DIV => left / right,
			POW => left.powf(right),
		}
	}
}
