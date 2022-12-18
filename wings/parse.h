#pragma once
#include "common.h"
#include "exprparse.h"

#include <string>
#include <vector>
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
			Try, Except, Finally, Raise,
			Break, Continue,
			Composite,
			Import, ImportFrom,
		} type;

		SourcePosition srcPos;
		Expression expr;
		std::vector<Statement> body;
		std::unique_ptr<Statement> elseClause;

		struct {
			size_t finallyCount;
			bool exitForNormally = false;
		} exitBlock;
		struct {
			AssignTarget assignTarget;
		} forLoop;
		struct {
			std::string name;
		} capture;
		struct {
			std::string name;
			std::vector<std::string> methodNames;
			std::vector<Expression> bases;
		} klass;
		struct {
			std::vector<Statement> exceptClauses;
			std::vector<Statement> finallyClause;
		} tryBlock;
		struct {
			std::string var;
			std::optional<Expression> exceptType;
		} exceptBlock;
		struct {
			std::string module;
			std::string alias;
		} import;
		struct {
			std::string module;
			std::vector<std::string> names;
			std::string alias;
		} importFrom;
	};

	struct ParseResult {
		CodeError error;
		Statement parseTree; // Root is treated similar to a def
	};

	ParseResult Parse(const LexTree& lexTree);

	std::unordered_set<std::string> GetReferencedVariables(const Expression& expr);
	CodeError ParseParameterList(TokenIter& p, std::vector<Parameter>& out);
	CodeError ParseForLoopVariableList(TokenIter& p, std::vector<std::string>& vars, bool& isTuple);
	Statement TransformForToWhile(Statement forLoop);
	void ExpandCompositeStatements(std::vector<Statement>& statements);

}
