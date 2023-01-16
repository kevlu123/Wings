#pragma once
#include "common.h"
#include "exprparse.h"

#include <string>
#include <vector>
#include <optional>
#include <unordered_set>
#include <memory>

namespace wings {

	namespace stat {
		struct Root {
			Expression expr;
		};
		
		struct Def {
			Expression expr;
		};

		struct Class {
			std::string name;
			std::vector<std::string> methodNames;
			std::vector<Expression> bases;
			std::vector<Statement> body;
		};
		
		struct Pass {};
		
		struct Expr {
			Expression expr;
		};

		struct Else {
			std::vector<Statement> body;
		};

		struct If {
			Expression expr;
			std::vector<Statement> body;
			std::unique_ptr<Statement> elseClause;
		};

		struct Elif {
			Expression expr;
			std::vector<Statement> body;
			std::unique_ptr<Statement> elseClause;
		};
		
		struct While {
			Expression expr;
			std::vector<Statement> body;
			std::unique_ptr<Statement> elseClause;
		};

		struct For {
			Expression expr;
			AssignTarget assignTarget;
			std::vector<Statement> body;
			std::unique_ptr<Statement> elseClause;
		};

		struct Try {
			std::vector<Statement> body;
			std::vector<Statement> exceptBlocks;
			std::vector<Statement> finallyBody;
		};

		struct Except {
			std::vector<Statement> body;
			std::optional<Expression> type;
			std::string variable;
		};

		struct Finally {
			std::vector<Statement> body;
		};

		struct Raise {
			Expression expr;
		};

		struct Import {
			std::string module;
			std::string alias;
		};

		struct ImportFrom {
			std::string module;
			std::vector<std::string> names;
			std::string alias;
		};

		struct Break {
			size_t finallyCount = 0;
			bool exitForLoopNormally = false;
		};

		struct Continue {
			size_t finallyCount = 0;
		};

		struct Return {
			size_t finallyCount = 0;
			Expression expr;
		};

		struct Composite {
			std::vector<Statement> body;
		};

		struct NonLocal {
			std::string name;
		};

		struct Global {
			std::string name;
		};
	}

	using StatData = std::variant<
		std::monostate,
		stat::Root,
		stat::Def,
		stat::Class,
		stat::Pass,
		stat::Expr,
		stat::If,
		stat::Elif,
		stat::Else,
		stat::While,
		stat::For,
		stat::Try,
		stat::Except,
		stat::Finally,
		stat::Raise,
		stat::Import,
		stat::ImportFrom,
		stat::Break,
		stat::Continue,
		stat::Return,
		stat::Composite,
		stat::NonLocal,
		stat::Global
	>;

	struct Statement {
		SourcePosition srcPos;
		StatData data;
		
		template <class T> T& Get() { return std::get<T>(data); }
		template <class T> const T& Get() const { return std::get<T>(data); }
		template <class T> T* GetIf() { return std::get_if<T>(&data); }
		template <class T> const T* GetIf() const { return std::get_if<T>(&data); }
		template <class T> bool Is() const { return std::holds_alternative<T>(data); }
	};

	template <class T>
	constexpr size_t StatIndex() {
		return variant_index<StatData, T>();
	}

	struct ParseResult {
		CodeError error;
		stat::Root parseTree;
	};

	ParseResult Parse(const LexTree& lexTree);

	std::unordered_set<std::string> GetReferencedVariables(const Expression& expr);
	CodeError ParseParameterList(TokenIter& p, std::vector<Parameter>& out);
	CodeError ParseForLoopVariableList(TokenIter& p, std::vector<std::string>& vars, bool& isTuple);
	Statement TransformForToWhile(stat::For forLoop);
	void ExpandCompositeStatements(std::vector<Statement>& statements);

}
