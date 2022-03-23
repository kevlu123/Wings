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
	static CodeError ParseBreak(const LexTree& node, Statement& out);
	static CodeError ParseContinue(const LexTree& node, Statement& out);
	static CodeError ParseDef(const LexTree& node, Statement& out);
	static CodeError ParseReturn(const LexTree& node, Statement& out);
	static CodeError ParsePass(const LexTree& node, Statement& out);

	static CodeError ParseNonlocal(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		++p;

		if (p.EndReached()) {
			return CodeError::Bad("Expected a variable name", (--p)->srcPos);
		} else if (p->type != Token::Type::Word) {
			return CodeError::Bad("Expected a variable name", p->srcPos);
		}

		out.type = Statement::Type::Nonlocal;
		out.capture.name = p->text;
		++p;
		return CheckTrailingTokens(p);
	}

	static CodeError ParseGlobal(const LexTree& node, Statement& out) {
		// Almost exactly tthe same as ParseNonLocal()
		if (auto error = ParseNonlocal(node, out)) {
			return error;
		} else {
			out.type = Statement::Type::Global;
			return CodeError::Good();
		}
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
		result.error = ParseBody(lexTree, result.parseTree.body);
		return result;
	}
}
