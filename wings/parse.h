#pragma once
#include "lex.h"
#include <memory>
#include <optional>

namespace wings {

	enum class Operation {
		Literal, Variable,
		ListLiteral, MapLiteral,
		ListComprehension,
		Index, Call,
		Add, Sub, Mul, Div, IDiv, Mod, Pow,
		Eq, Ne, Lt, Le, Gt, Ge,
		And, Or, Not,
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
	};

	struct Parameter {
		std::optional<Expression> defaultValue;
	};

	struct Capture {
		std::string name;
		bool global;
	};

	struct Statement {
		enum class Type {
			Expr,
			Def, Return,
			If, Elif, Else,
			While, For,
			Break, Continue,
		} type;

		// Main expression argument
		Expression expr;

		struct {
			Expression variables;
		} forLoop;
		struct {
			std::vector<Parameter> parameters;
			std::vector<Capture> captures;
			std::vector<Statement> body;
		} def;
	};

	struct ParseResult {
		CodeError error;
		std::vector<Statement> statements;
	};

	ParseResult Parse(const LexTree& lexTree);

}
