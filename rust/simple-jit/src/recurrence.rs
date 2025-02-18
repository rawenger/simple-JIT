#![allow(unused)]  // TODO: remove!!
use crate::ast::AST;
use anyhow::{anyhow,bail,Result};
use derive_more::Display;

pub type NumType = f64;

const SYMBOLS: [char; 7] = ['+', '-', '*', '/', '^', '(', ')'];


pub struct Recurrence {
	eqn: Vec<Token>,
}

impl Recurrence {
	pub fn new<S: AsRef<str>>(eqn: S) -> Result<Recurrence> {
		Ok(Self {
			eqn: to_postfix(eqn.as_ref().trim())?
		})
	}

	pub fn compute(&self, x0: NumType, n_iter: usize) -> NumType {
		dbg!(&self.eqn);
		let mut calc = Vec::new();
		let mut n_last = x0;

		for _ in 0..n_iter {
			for tok in &self.eqn {
				match tok {
					Token::NUM(n) => calc.push(*n),
					Token::ALPHA(_) => calc.push(n_last),
					tok => {
						let n2 = calc.pop().unwrap();
						let n1 = calc.pop().unwrap();
						let compute = match tok {
							Token::PLUS => n1 + n2,
							Token::MINUS => n1 - n2,
							Token::TIMES => n1 * n2,
							Token::DIV => n1 / n2,
							Token::POW => n1.powf(n2),
							_ => panic!("unknown error occurred")
						};
						calc.push(compute);
					}
				}
			}
			n_last = calc.pop().unwrap();
		}
		n_last
	}

	pub fn make_ast(&self) -> Result<AST> {
		AST::construct(&self.eqn)
	}
}

fn to_postfix(eqn: &str) -> Result<Vec<Token>> {
	/* postfix algorithm
	 * - iterate over tokens, maintaining stack of operators as we go.
	 * - when we see an operand, add to pf vec
	 * - when we see an operator, first pop from stack until
	 *   top has lower (not equal) precedence, stack becomes empty,
	 *   or top is LPAREN. then push new operator to stack.
	 *
	 * - when we see LPAREN, push to stack
	 * - when we see RPAREN, pop from stack until we reach LPAREN 
	 *   (if stack empties before this point we have mismatched parens)
	 *
	 * when done, append the rest of the opstack to the pfix
	 * sequence (still in LIFO order)
	 */

	let mut stream = eqn;
	let mut tok;

	let mut pfix = Vec::new();
	let mut opstack: Vec<Token> = Vec::new();

	while !stream.is_empty() {
		(tok, stream) = Token::from_stream(stream)?;
		// dbg!(&tok);
		match tok {
			Token::ALPHA(_) | Token::NUM(_) => pfix.push(tok),
			Token::LPAREN => opstack.push(tok),
			Token::RPAREN => {
				loop {
					let op = opstack.pop().ok_or_else(|| anyhow!("malformed expression"))?;
					if op == Token::LPAREN {
						break;
					}
					pfix.push(op);
				}
			},
			_ => {
				while let Some(op) = opstack.last() {
					if *op == Token::LPAREN || op.precedence() < tok.precedence() {
						break;
					}
					pfix.push(opstack.pop().unwrap());
				}
				opstack.push(tok);
			}
		}
		// dbg!(&opstack);
	}

	while let Some(op) = opstack.pop() {
		pfix.push(op);
	}

	Ok(pfix)
}

#[derive(Debug, PartialEq)]
pub enum Token {
	LPAREN,
	RPAREN,
	PLUS, MINUS, TIMES, DIV, POW,
	// for now, just variable name. possibly later used to support builtin functions (e.g. sin/cos)
	ALPHA(String),
	NUM(NumType)
}

impl Token {
	/// parses & returns the next token from `stream`, returning
	/// the new token and the remaining (trimmed) characters from `stream`
	///
	/// NOTE: `stream` is expected to have no leading whitespace.
	/// this function will panic if it is empty.
	pub fn from_stream(stream: &str) -> Result<(Self, &str)> {
		let mut consumed = 0_usize;
		let mut stream = stream;

		// there's no way to "put back" characters into an iterator in a
		//  way that allows us to maintain our view over the original `str`
		//  object (and thus not have to `collect()` into a return value)
		let tok = match stream.chars().next().unwrap()
		{
			s if SYMBOLS.contains(&s) => {
				stream = stream.get(1..).unwrap_or("");
				match s {
					'(' => Self::LPAREN,
					')' => Self::RPAREN,
					'+' => Self::PLUS,
					'-' => Self::MINUS,
					'*' => Self::TIMES,
					'/' => Self::DIV,
					'^' => Self::POW,
					_ => panic!("how did we get here??"),
				}
			},

			a if a.is_alphabetic() => {
				let mut consumed = 0_usize;
				let alph = stream.chars()
								.take_while(|c| c.is_alphabetic())
								.collect::<String>();
				stream = stream.trim_start_matches(&alph);
				Self::ALPHA(alph)
			},

			d if d.is_ascii_digit() || d == '.' => {
				let mut consumed = 0_usize;
				let num = stream.chars()
								.take_while(|c| c.is_ascii_digit() || *c == '.')
								.collect::<String>();
				stream = stream.trim_start_matches(&num);
				Self::NUM(num.parse::<NumType>()?)
			},
			inval => { bail!("invalid character '{inval}' in equation"); }
		};

		Ok((tok, stream.trim()))
		/* `Token` value */
	}

	/// returns operator precedence of the token (if applicable)
	/// 0 = lowest precedence, etc
	pub fn precedence(&self) -> u32 {
		use Token::*;
		match self {
			PLUS | MINUS => 0,
			TIMES | DIV => 1,
			POW => 2,
			_ => panic!("cannot compute precedence for {self:?}")
		}
	}
}