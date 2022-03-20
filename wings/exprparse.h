#pragma once
#include "lex.h"
#include <vector>

namespace wings {

	enum class Operation {
		Literal, Variable,
		ListLiteral, MapLiteral,
		ListComprehension,
		Index, Call,
		Pos, Neg,
		Add, Sub, Mul, Div, IDiv, Mod, Pow,
		Eq, Ne, Lt, Le, Gt, Ge,
		And, Or, Not,
		In, NotIn,
		BitAnd, BitOr, BitNot, BitXor,
		ShiftL, ShiftR,
		Assign, AddAssign, SubAssign, MulAssign,
		DivAssign, IDivAssign, ModAssign, PowAssign,
		AndAssign, OrAssign, XorAssign,
		ShiftLAssign, ShiftRAssign,
		Incr, Decr,
	};

	struct Expression {
		Operation operation;
		std::vector<Expression> children;
		Token literal;
	};

	CodeError ParseExpression(const std::vector<Token>& tokens, Expression& out);

}
