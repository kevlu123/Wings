#pragma once
#include "lex.h"
#include <vector>

namespace wings {

	enum class Operation {
		Literal, Variable,
		Tuple, List, Map,
		ListComprehension,
		Index, Call,
		Pos, Neg,
		Add, Sub, Mul, Div, IDiv, Mod, Pow,
		Eq, Ne, Lt, Le, Gt, Ge,
		And, Or, Not,
		In, NotIn,
		BitAnd, BitOr, BitNot, BitXor,
		ShiftL, ShiftR,
		IfElse,
		Assign, AddAssign, SubAssign, MulAssign,
		DivAssign, IDivAssign, ModAssign, PowAssign,
		AndAssign, OrAssign, XorAssign,
		ShiftLAssign, ShiftRAssign,
		Incr, Decr,
		Dot,
	};

	enum class AssignType {
		None,
		// var = value
		Direct,
		// var[index] = value
		Index,
		// var.member = value
		Member,
	};

	struct LiteralValue {
		enum class Type {
			Null,
			Bool,
			Int,
			Float,
			String,
		} type;

		struct {
			union {
				bool b;
				wint i;
				wfloat f;
			};
			std::string s;
		};
	};

	struct Expression {
		Operation operation;
		AssignType assignType = AssignType::None;
		std::vector<Expression> children;
		SourcePosition srcPos;

		std::string variableName;
		LiteralValue literalValue;
	};

	struct TokenIter {
		TokenIter(const std::vector<Token>& tokens);
		TokenIter& operator++();
		TokenIter& operator--();
		const Token& operator*() const;
		const Token* operator->() const;
		bool operator==(const TokenIter& rhs) const;
		bool operator!=(const TokenIter& rhs) const;
		bool EndReached() const;
	private:
		size_t index;
		const std::vector<Token>* tokens;
	};

	CodeError ParseExpression(TokenIter& p, Expression& out, bool disableInOp = false);

}
