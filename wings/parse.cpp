#include "parse.h"
#include <unordered_map>

namespace wings {

	static CodeError CheckTrailingTokens(const TokenIter& iter) {
		if (!iter.EndReached()) {
			return CodeError::Bad("Unexpected trailing tokens", iter->srcPos);
		} else {
			return CodeError::Good();
		}
	}

	using ParseFn = CodeError(*)(const LexTree& node, Statement& out);

	static CodeError ParseIf(const LexTree& node, Statement& out);
	static CodeError ParseElif(const LexTree& node, Statement& out);
	static CodeError ParseElse(const LexTree& node, Statement& out);
	static CodeError ParseWhile(const LexTree& node, Statement& out);
	static CodeError ParseFor(const LexTree& node, Statement& out);
	static CodeError ParseDef(const LexTree& node, Statement& out);
	static CodeError ParseReturn(const LexTree& node, Statement& out);

	static CodeError ParseSingleToken(const LexTree& node, Statement& out, Statement::Type type) {
		TokenIter p(node.tokens);
		++p;
		out.type = type;
		return CheckTrailingTokens(p);
	}

	static CodeError ParseBreak(const LexTree& node, Statement& out) {
		return ParseSingleToken(node, out, Statement::Type::Break);
	}

	static CodeError ParseContinue(const LexTree& node, Statement& out) {
		return ParseSingleToken(node, out, Statement::Type::Continue);
	}

	static CodeError ParsePass(const LexTree& node, Statement& out) {
		return ParseSingleToken(node, out, Statement::Type::Pass);
	}

	static CodeError ParseCapture(const LexTree& node, Statement& out, Statement::Type type) {
		TokenIter p(node.tokens);
		++p;

		if (p.EndReached()) {
			return CodeError::Bad("Expected a variable name", (--p)->srcPos);
		} else if (p->type != Token::Type::Word) {
			return CodeError::Bad("Expected a variable name", p->srcPos);
		}

		out.type = type;
		out.capture.name = p->text;
		++p;
		return CheckTrailingTokens(p);
	}

	static CodeError ParseNonlocal(const LexTree& node, Statement& out) {
		return ParseCapture(node, out, Statement::Type::Nonlocal);
	}

	static CodeError ParseGlobal(const LexTree& node, Statement& out) {
		return ParseCapture(node, out, Statement::Type::Global);
	}

	// Get a set of variables referenced by an expression
	static std::unordered_set<std::string> GetReferencedVariables(const Expression& expr) {
		std::unordered_set<std::string> variables;
		if (expr.operation == Operation::Variable) {
			variables.insert(expr.literal.text);
		} else {
			for (const auto& child : expr.children) {
				variables.merge(GetReferencedVariables(child));
			}
		}
		return variables;
	}

	// Get a set of variables directly written to by the '=' operator. This excludes compound assignment.
	static std::unordered_set<std::string> GetWriteVariables(const Expression& expr) {
		std::unordered_set<std::string> variables;
		if (expr.operation == Operation::Assign && expr.children[0].operation == Operation::Variable) {
			variables.insert(expr.children[0].literal.text);
		} else {
			for (const auto& child : expr.children) {
				variables.merge(GetWriteVariables(child));
			}
		}
		return variables;
	}

	static CodeError ParseExpressionStatement(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		out.type = Statement::Type::Expr;
		if (auto error = ParseExpression(p, out.expr)) {
			return error;
		} else {
			return CheckTrailingTokens(p);
		}
	}

	static const std::unordered_map<std::string, ParseFn> STATEMENT_STARTINGS = {
		{ "if", ParseIf },
		{ "elif", ParseElif },
		{ "else", ParseElse },
		{ "while", ParseWhile },
		{ "for", ParseFor },
		{ "break", ParseBreak },
		{ "continue", ParseContinue },
		{ "def", ParseDef },
		{ "return", ParseReturn },
		{ "pass", ParsePass },
		{ "nonlocal", ParseNonlocal },
		{ "global", ParseGlobal },
	};

	static CodeError ParseStatement(const LexTree& node, Statement& out) {
		const auto& firstToken = node.tokens.at(0).text;
		if (STATEMENT_STARTINGS.contains(firstToken)) {
			return STATEMENT_STARTINGS.at(firstToken)(node, out);
		} else {
			return ParseExpressionStatement(node, out);
		}
	}

	static CodeError ParseBody(const LexTree& node, std::vector<Statement>& out) {
		out.clear();
		for (auto& node : node.children) {
			Statement statement;
			if (auto error = ParseStatement(node, statement)) {
				out.clear();
				return error;
			}
			out.push_back(std::move(statement));
		}
		return CodeError::Good();
	}

	ParseResult Parse(const LexTree& lexTree) {
		ParseResult result{};
		result.parseTree.type = Statement::Type::Def;
		result.error = ParseBody(lexTree, result.parseTree.body);
		return result;
	}
}
