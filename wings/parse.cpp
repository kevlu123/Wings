#include "parse.h"
#include "impl.h"
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

	static Statement TransformForToWhile(Statement forLoop) {
		// __VarXXX = expression
		std::string rangeVarName = "__For" + std::to_string(Guid());
		Expression rangeVar{};
		rangeVar.operation = Operation::Variable;
		rangeVar.literal.type = Token::Type::Word;
		rangeVar.literal.literal.s = rangeVarName;
		rangeVar.literal.text = rangeVarName;

		Statement rangeEval{};
		rangeEval.type = Statement::Type::Expr;
		rangeEval.expr.operation = Operation::Assign;
		rangeEval.expr.children.push_back(rangeVar);
		rangeEval.expr.children.push_back(std::move(forLoop.expr));

		// while not iterend(__VarXXX):
		Expression loadEndCheck{};
		loadEndCheck.operation = Operation::Variable;
		loadEndCheck.literal.type = Token::Type::Word;
		loadEndCheck.literal.literal.s = "iterend";
		loadEndCheck.literal.text = "iterend";

		Expression callEndCheck{};
		callEndCheck.operation = Operation::Call;
		callEndCheck.children.push_back(std::move(loadEndCheck));
		callEndCheck.children.push_back(rangeVar);

		Expression condition{};
		condition.operation = Operation::Not;
		condition.children.push_back(std::move(callEndCheck));

		Statement wh{};
		wh.type = Statement::Type::While;
		wh.expr = std::move(condition);

		// vars = iternext(__VarXXX)
		Expression loadNext{};
		loadNext.operation = Operation::Variable;
		loadNext.literal.type = Token::Type::Word;
		loadNext.literal.literal.s = "iternext";
		loadNext.literal.text = "iternext";

		Expression callNext{};
		callNext.operation = Operation::Call;
		callNext.children.push_back(std::move(loadNext));
		callNext.children.push_back(std::move(rangeVar));

		Expression iterAssign{};
		iterAssign.operation = Operation::Assign;
		iterAssign.children.push_back(std::move(forLoop.forLoop.variable));
		iterAssign.children.push_back(std::move(callNext));

		Statement iterAssignStat{};
		iterAssignStat.type = Statement::Type::Expr;
		iterAssignStat.expr = std::move(iterAssign);
		wh.body.push_back(std::move(iterAssignStat));

		// Transfer body over
		for (auto& child : forLoop.body)
			wh.body.push_back(std::move(child));

		Statement out{};
		out.type = Statement::Type::Composite;
		out.body.push_back(std::move(rangeEval));
		out.body.push_back(std::move(wh));
		return out;
	}

	static CodeError ParseFor(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		++p;
		out.type = Statement::Type::For;

		if (auto error = ParseExpression(p, out.forLoop.variable, true)) {
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

		if (auto error = ParseBody(node, Statement::Type::For, out.body)) {
			return error;
		}

		out = TransformForToWhile(std::move(out));
		return CodeError::Good();
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

			// Check for duplicate parameters
			if (std::find_if(out.begin(), out.end(), [&](const Parameter& p) {
				return p.name == parameterName;
				}) != out.end()) {
				return CodeError::Bad("Duplicate parameter name", p->srcPos);
			}
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
			} if (!out.empty() && out.back().defaultValue) {
				// If last parameter has a default value,
				// this parameter must also have a default value
				return CodeError::Bad(
					"Parameters with default values must appear at the end of the parameter list",
					(--p)->srcPos
				);
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
	static std::unordered_set<T> SetDifference(const std::unordered_set<T>& set, const Subtract& subtract, const Args&... args) {
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

		out.type = Statement::Type::Return;
		if (p.EndReached()) {
			out.expr.operation = Operation::Literal;
			out.expr.literal.type = Token::Type::Null;
			return CodeError::Good();
		} else if (auto error = ParseExpression(p, out.expr)) {
			return error;
		} else {
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
		const auto& firstToken = node.tokens[0].text;
		if (STATEMENT_STARTINGS.contains(firstToken)) {
			if (auto error = STATEMENT_STARTINGS.at(firstToken)(node, out)) {
				return error;
			}
		} else {
			if (auto error = ParseExpressionStatement(node, out)) {
				return error;
			}
		}

		out.token = node.tokens[0];
		return CodeError::Good();
	}

	static CodeError ParseBody(const LexTree& node, Statement::Type statType, std::vector<Statement>& out) {
		out.clear();

		if (node.children.empty()) {
			return CodeError::Bad("Expected a statement", node.tokens.back().srcPos);
		}

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

		// Expand composite statements
		for (size_t i = 0; i < out.size(); i++) {
			if (out[i].type == Statement::Type::Composite) {
				for (auto& child : out[i].body) {
					out.insert(out.begin() + i + 1, std::move(child));
				}
				out.erase(out.begin() + i);
			}
		}

		// Rearrange elif and else nodes
		for (size_t i = 0; i < out.size(); i++) {
			auto& stat = out[i];
			Statement::Type lastType = i ? out[i - 1].type : Statement::Type::Pass;

			std::optional<Statement> elseClause;
			if (stat.type == Statement::Type::Elif) {
				if (lastType != Statement::Type::If && lastType != Statement::Type::Elif) {
					return CodeError::Bad(
						"An 'elif' clause may only appear after an 'if' or 'elif' clause",
						stat.token.srcPos
					);
				}

				// Transform elif into an else and if statement
				stat.type = Statement::Type::If;
				elseClause = Statement{};
				elseClause.value().type = Statement::Type::Else;
				elseClause.value().body.push_back(std::move(stat));
				out.erase(out.begin() + i);
				i--;

			} else if (stat.type == Statement::Type::Else) {
				if (lastType != Statement::Type::If
					&& lastType != Statement::Type::Elif
					&& lastType != Statement::Type::While)
				{
					return CodeError::Bad(
						"An 'else' clause may only appear after an 'if', 'elif', 'while', or 'for' clause",
						stat.token.srcPos
					);
				}

				elseClause = std::move(stat);
				out.erase(out.begin() + i);
				i--;
			}

			if (elseClause) {
				Statement* parent = &out[i];
				while (parent->elseClause) {
					parent = &parent->elseClause->body.back();
				}
				parent->elseClause = std::make_unique<Statement>(std::move(elseClause.value()));
			}
		}

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
