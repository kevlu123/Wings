#pragma once
#include "exprparse.h"
#include <optional>

namespace wings {

	struct Parameter {
		std::optional<Expression> defaultValue;
	};

	struct Capture {
		std::string name;
		bool global;
	};

	struct Statement {
		enum class Type {
			Pass,
			Expr,
			Nonlocal, Global,
			Def, Return,
			If, Elif, Else,
			While, For,
			Break, Continue,
		} type;

		// Main expression argument
		Expression expr;

		std::vector<Statement> body;

		struct {
			Expression variables;
		} forLoop;
		struct {
			std::vector<Parameter> parameters;
			std::vector<Capture> captures;
			std::vector<std::string> variables;
		} def;
	};

	struct ParseResult {
		CodeError error;
		Statement parseTree; // Root is treated similar to a def
	};

	ParseResult Parse(const LexTree& lexTree);

}
