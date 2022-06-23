#pragma once
#include "exprparse.h"
#include <optional>
#include <unordered_set>
#include <memory>

namespace wings {

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
			AssignTarget assignTarget;
		} forLoop;
		struct {
			std::string name;
		} capture;
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

	std::unordered_set<std::string> GetReferencedVariables(const Expression& expr);
	CodeError ParseParameterList(TokenIter& p, std::vector<Parameter>& out);

}
