#include "parse.h"
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <iterator>

namespace wings {

	thread_local std::vector<Statement::Type> statementHierarchy;

	static CodeError ParseBody(const LexTree& node, Statement::Type statType, std::vector<Statement>& out);

	static CodeError CheckTrailingTokens(const TokenIter& iter) {
		if (!iter.EndReached()) {
			return CodeError::Bad("Unexpected trailing tokens", iter->srcPos);
		} else {
			return CodeError::Good();
		}
	}

	static CodeError ExpectColonEnding(TokenIter& p) {
		if (p.EndReached()) {
			return CodeError::Bad("Expected a ':'", (--p)->srcPos);
		} else if (p->text != ":") {
			return CodeError::Bad("Expected a ':'", p->srcPos);
		}
		++p;

		return CheckTrailingTokens(p);
	}

	static CodeError ParseConditionalBlock(const LexTree& node, Statement& out, Statement::Type type) {
		TokenIter p(node.tokens);
		++p;

		if (auto error = ParseExpression(p, out.expr)) {
			return error;
		}

		if (auto error = ExpectColonEnding(p)) {
			return error;
		}

		out.type = type;
		return ParseBody(node, type, out.body);
	}

	static CodeError ParseIf(const LexTree& node, Statement& out) {
		return ParseConditionalBlock(node, out, Statement::Type::If);
	}

	static CodeError ParseElif(const LexTree& node, Statement& out) {
		return ParseConditionalBlock(node, out, Statement::Type::Elif);
	}

	static CodeError ParseElse(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		++p;

		out.type = Statement::Type::Else;
		if (auto error = ExpectColonEnding(p)) {
			return error;
		}

		return ParseBody(node, Statement::Type::Else, out.body);
	}

	static CodeError ParseWhile(const LexTree& node, Statement& out) {
		return ParseConditionalBlock(node, out, Statement::Type::While);
	}

	static CodeError ParseVariableList(TokenIter& p, std::vector<std::string>& out) {
		out.clear();
		if (p.EndReached()) {
			return CodeError::Bad("Expected a variable name", (--p)->srcPos);
		} else if (p->type != Token::Type::Word) {
			return CodeError::Bad("Expected a variable name", p->srcPos);
		}

		while (true) {
			out.push_back(p->text);
			++p;

			if (p.EndReached()) {
				return CodeError::Good();
			} else if (p->text != ",") {
				return CodeError::Good();
			} 
			++p;

			if (p.EndReached()) {
				return CodeError::Good();
			} else if (p->type != Token::Type::Word) {
				return CodeError::Good();
			}
		}
	}

	static CodeError ParseFor(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		++p;
		out.type = Statement::Type::For;

		if (auto error = ParseVariableList(p, out.forLoop.variables)) {
			return error;
		}

		if (p.EndReached()) {
			return CodeError::Bad("Expected a 'in'");
		}
		++p;

		if (auto error = ParseExpression(p, out.expr)) {
			return error;
		}

		if (auto error = ExpectColonEnding(p)) {
			return error;
		}

		return ParseBody(node, Statement::Type::For, out.body);
	}

	static CodeError ParseParameterList(TokenIter& p, std::vector<Parameter>& out) {
		out.clear();
		while (true) {
			if (p.EndReached()) {
				return CodeError::Good();
			} else if (p->type != Token::Type::Word) {
				return CodeError::Good();
			}

			std::string parameterName = p->text;
			std::optional<Expression> defaultValue;
			++p;

			if (p.EndReached()) {
				out.push_back(Parameter{ parameterName });
				return CodeError::Good();
			} else if (p->text == "=") {
				// Default value
				++p;
				Expression expr{};
				if (auto error = ParseExpression(p, expr)) {
					return error;
				}
				defaultValue = std::move(expr);
			} else if (!out.empty() && out.back().defaultValue) {
				// If last parameter has a default value,
				// this parameter must also have a default value
				return CodeError::Bad(
					"Parameters with default values must appear at the end of the parameter list",
					(--p)->srcPos
				);
			}

			// Check for duplicate parameters
			if (std::find_if(out.begin(), out.end(), [&](const Parameter& p) {
				return p.name == parameterName;
				}) != out.end()) {
				return CodeError::Bad("Duplicate parameter name", p->srcPos);
			}

			out.push_back(Parameter{ std::move(parameterName), std::move(defaultValue) });

			if (p.EndReached()) {
				return CodeError::Good();
			} else if (p->text != ",") {
				return CodeError::Good();
			}
			++p;
		}
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

	template <typename T, typename Subtract, typename... Args>
	std::unordered_set<T> SetDifference(const std::unordered_set<T>& set, const Subtract& subtract, const Args&... args) {
		if constexpr (sizeof...(args) == 0) {
			std::unordered_set<T> diff = set;
			for (const auto& sub : subtract)
				diff.erase(sub);
			return diff;
		} else {
			return SetDifference(SetDifference(set, subtract), args...);
		}
	}

	static void ResolveCaptures(Statement& defNode) {
		std::unordered_set<std::string> writeVars;
		std::unordered_set<std::string> allVars;

		std::function<void(const Statement&)> scanNode = [&](const Statement& node) {
			for (const auto& child : node.body) {
				switch (child.type) {
				case Statement::Type::Expr:
				case Statement::Type::If:
				case Statement::Type::Elif:
				case Statement::Type::While:
				case Statement::Type::Return:
					writeVars.merge(GetWriteVariables(child.expr));
					allVars.merge(GetReferencedVariables(child.expr));
					break;
				case Statement::Type::For: {
					writeVars.merge(GetWriteVariables(child.expr));
					allVars.merge(GetReferencedVariables(child.expr));

					const auto& loopVars = child.forLoop.variables;
					writeVars.insert(loopVars.begin(), loopVars.end());
					allVars.insert(loopVars.begin(), loopVars.end());
					break;
				}
				case Statement::Type::Def:
					writeVars.insert(child.def.name);
					allVars.insert(child.def.name);

					for (const auto& parameter : child.def.parameters) {
						if (parameter.defaultValue) {
							writeVars.merge(GetWriteVariables(parameter.defaultValue.value()));
							allVars.merge(GetReferencedVariables(parameter.defaultValue.value()));
						}
					}

					allVars.insert(child.def.localCaptures.begin(), child.def.localCaptures.end());
					break;
				case Statement::Type::Global:
					defNode.def.globalCaptures.insert(child.capture.name);
					break;
				case Statement::Type::Nonlocal:
					defNode.def.localCaptures.insert(child.capture.name);
					break;
				}

				if (child.type != Statement::Type::Def) {
					scanNode(child);
				}
			}
		};

		scanNode(defNode);

		std::vector<std::string> parameterVars;
		for (const auto& param : defNode.def.parameters)
			parameterVars.push_back(param.name);
		defNode.def.localCaptures.merge(SetDifference(allVars, writeVars, parameterVars));
		defNode.def.variables = SetDifference(writeVars, defNode.def.globalCaptures, defNode.def.localCaptures);
	}

	static CodeError ParseDef(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		++p;
		out.type = Statement::Type::Def;

		if (p.EndReached()) {
			return CodeError::Bad("Expected a function name", (--p)->srcPos);
		} else if (p->type != Token::Type::Word) {
			return CodeError::Bad("Expected a function name", p->srcPos);
		}
		out.def.name = p->text;
		++p;

		if (p.EndReached()) {
			return CodeError::Bad("Expected a '('", (--p)->srcPos);
		} else if (p->text != "(") {
			return CodeError::Bad("Expected a '('", p->srcPos);
		}
		++p;

		if (auto error = ParseParameterList(p, out.def.parameters)) {
			return error;
		}

		if (p.EndReached()) {
			return CodeError::Bad("Expected a ')'", (--p)->srcPos);
		} else if (p->text != ")") {
			return CodeError::Bad("Expected a ')'", p->srcPos);
		}
		++p;

		if (auto error = ExpectColonEnding(p)) {
			return error;
		}

		if (auto error = ParseBody(node, Statement::Type::Def, out.body)) {
			return error;
		}

		ResolveCaptures(out);

		return CodeError::Good();
	}

	static CodeError ParseReturn(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		++p;

		if (auto error = ParseExpression(p, out.expr)) {
			return error;
		} else {
			out.type = Statement::Type::Return;
			return CheckTrailingTokens(p);
		}
	}

	static CodeError ParseSingleToken(const LexTree& node, Statement& out, Statement::Type type) {
		TokenIter p(node.tokens);
		++p;
		out.type = type;
		return CheckTrailingTokens(p);
	}

	static CodeError CheckBreakable(const LexTree& node) {
		auto it = statementHierarchy.rbegin();
		while (true) {
			if (*it == Statement::Type::Def || *it == Statement::Type::Root) {
				return CodeError::Bad("'break' or 'continue' outside of loop", node.tokens[0].srcPos);
			} else if (*it == Statement::Type::For || *it == Statement::Type::While) {
				return CodeError::Good();
			}
			++it;
		}
	}

	static CodeError ParseBreak(const LexTree& node, Statement& out) {
		if (auto error = CheckBreakable(node)) {
			return error;
		}
		return ParseSingleToken(node, out, Statement::Type::Break);
	}

	static CodeError ParseContinue(const LexTree& node, Statement& out) {
		if (auto error = CheckBreakable(node)) {
			return error;
		}
		return ParseSingleToken(node, out, Statement::Type::Continue);
	}

	static CodeError ParsePass(const LexTree& node, Statement& out) {
		return ParseSingleToken(node, out, Statement::Type::Pass);
	}

	static CodeError ParseCapture(const LexTree& node, Statement& out, Statement::Type type) {
		TokenIter p(node.tokens);
		++p;

		if (statementHierarchy.back() == Statement::Type::Root) {
			return CodeError::Bad("Cannot capture at top level", (--p)->srcPos);
		}

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

	static CodeError ParseExpressionStatement(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		out.type = Statement::Type::Expr;
		if (auto error = ParseExpression(p, out.expr)) {
			return error;
		} else {
			return CheckTrailingTokens(p);
		}
	}

	using ParseFn = CodeError(*)(const LexTree& node, Statement& out);

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

	static CodeError ParseBody(const LexTree& node, Statement::Type statType, std::vector<Statement>& out) {
		out.clear();
		statementHierarchy.push_back(statType);
		for (auto& node : node.children) {
			Statement statement;
			if (auto error = ParseStatement(node, statement)) {
				out.clear();
				return error;
			}
			out.push_back(std::move(statement));
		}
		statementHierarchy.pop_back();
		return CodeError::Good();
	}

	ParseResult Parse(const LexTree& lexTree) {
		ParseResult result{};
		result.parseTree.type = Statement::Type::Root;

		statementHierarchy.clear();
		result.error = ParseBody(lexTree, Statement::Type::Root, result.parseTree.body);
		statementHierarchy.clear();

		ResolveCaptures(result.parseTree);
		result.parseTree.def.variables.merge(result.parseTree.def.localCaptures);
		result.parseTree.def.localCaptures.clear();

		return result;
	}
}
