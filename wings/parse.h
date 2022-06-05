#pragma once
#include "exprparse.h"
#include <optional>
#include <unordered_set>
#include <memory>

namespace wings {

	struct Parameter {
		std::string name;
		std::optional<Expression> defaultValue;
	};

	struct Statement {
		enum class Type {
			Root,
			Pass,
			Expr,
			Nonlocal, Global,
			Def, Class, Return,
			If, Elif, Else,
			While, For,
			Break, Continue,
			Composite,
		} type;

		SourcePosition srcPos;
		Expression expr;
		std::vector<Statement> body;
		std::unique_ptr<Statement> elseClause;

		struct {
			Expression variable;
		} forLoop;
		struct {
			std::string name;
		} capture;
		struct {
			std::string name;
			std::vector<Parameter> parameters;
			std::unordered_set<std::string> globalCaptures;
			std::unordered_set<std::string> localCaptures;
			std::unordered_set<std::string> variables;
		} def;
		struct {
			std::string name;
			std::vector<std::string> methodNames;
		} _class;
	};

	struct ParseResult {
		CodeError error;
		Statement parseTree; // Root is treated similar to a def
	};

	ParseResult Parse(const LexTree& lexTree);

}
