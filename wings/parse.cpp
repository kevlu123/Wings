#include "parse.h"

#include <unordered_map>
#include <functional>
#include <algorithm>
#include <iterator>

namespace wings {

	static thread_local std::vector<Statement::Type> statementHierarchy;

	static CodeError ParseBody(const LexTree& node, Statement::Type statType, std::vector<Statement>& out);

	static CodeError CheckTrailingTokens(const TokenIter& p) {
		if (!p.EndReached()) {
			return CodeError::Bad("Unexpected trailing tokens", p->srcPos);
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

	Statement TransformForToWhile(Statement forLoop) {
		// __VarXXX = expression.__iter__()
		std::string rangeVarName = "__For" + std::to_string(Guid());

		Expression loadIter{};
		loadIter.srcPos = forLoop.expr.srcPos;
		loadIter.operation = Operation::Dot;
		loadIter.variableName = "__iter__";
		loadIter.children.push_back(std::move(forLoop.expr));

		Expression callIter{};
		callIter.srcPos = forLoop.expr.srcPos;
		callIter.operation = Operation::Call;
		callIter.children.push_back(std::move(loadIter));
		
		Statement rangeEval{};
		rangeEval.srcPos = forLoop.expr.srcPos;
		rangeEval.type = Statement::Type::Expr;
		rangeEval.expr.operation = Operation::Assign;
		rangeEval.expr.srcPos = forLoop.expr.srcPos;
		rangeEval.expr.assignTarget.type = AssignType::Direct;
		rangeEval.expr.assignTarget.direct = rangeVarName;
		rangeEval.expr.children.push_back({}); // Dummy
		rangeEval.expr.children.push_back(std::move(callIter));

		// while True:
		Expression condition{};
		condition.srcPos = forLoop.expr.srcPos;
		condition.operation = Operation::Literal;
		condition.literalValue.type = LiteralValue::Type::Bool;
		condition.literalValue.b = true;

		Statement wh{};
		wh.srcPos = forLoop.expr.srcPos;
		wh.type = Statement::Type::While;
		wh.expr = std::move(condition);

		// try:
		//		__VarXXX = __VarXXX.__next__()
		// except StopIteration:
		//		break
		Statement brk{};
		brk.srcPos = forLoop.expr.srcPos;
		brk.type = Statement::Type::Break;
		brk.exitBlock.finallyCount = 1;
		brk.exitBlock.exitForNormally = true;

		Expression stopIter{};
		stopIter.srcPos = forLoop.expr.srcPos;
		stopIter.operation = Operation::Variable;
		stopIter.variableName = "StopIteration";

		Statement except{};
		except.srcPos = forLoop.expr.srcPos;
		except.type = Statement::Type::Except;
		except.exceptBlock.exceptType = std::move(stopIter);
		except.body.push_back(std::move(brk));

		Statement tryExcept{};
		tryExcept.srcPos = forLoop.expr.srcPos;
		tryExcept.type = Statement::Type::Try;
		tryExcept.tryBlock.exceptClauses.push_back(std::move(except));

		// vars = __VarXXX.__next__()
		Expression rangeVar{};
		rangeVar.srcPos = forLoop.expr.srcPos;
		rangeVar.operation = Operation::Variable;
		rangeVar.variableName = rangeVarName;

		Expression loadNext{};
		loadNext.srcPos = forLoop.expr.srcPos;
		loadNext.operation = Operation::Dot;
		loadNext.variableName = "__next__";
		loadNext.children.push_back(std::move(rangeVar));

		Expression callNext{};
		callNext.srcPos = forLoop.expr.srcPos;
		callNext.operation = Operation::Call;
		callNext.children.push_back(std::move(loadNext));

		Expression iterAssign{};
		iterAssign.srcPos = forLoop.expr.srcPos;
		iterAssign.operation = Operation::Assign;
		iterAssign.assignTarget = forLoop.forLoop.assignTarget;
		iterAssign.children.push_back({}); // Dummy
		iterAssign.children.push_back(std::move(callNext));

		Statement iterAssignStat{};
		iterAssignStat.srcPos = forLoop.expr.srcPos;
		iterAssignStat.type = Statement::Type::Expr;
		iterAssignStat.expr = std::move(iterAssign);
		tryExcept.body.push_back(std::move(iterAssignStat));

		// Transfer body over
		wh.body.push_back(std::move(tryExcept));
		for (auto& child : forLoop.body)
			wh.body.push_back(std::move(child));

		Statement out{};
		out.srcPos = forLoop.expr.srcPos;
		out.type = Statement::Type::Composite;
		out.body.push_back(std::move(rangeEval));
		out.body.push_back(std::move(wh));
		return out;
	}

	CodeError ParseForLoopVariableList(TokenIter& p, std::vector<std::string>& vars, bool& isTuple) {
		bool mustTerminate = false;
		isTuple = false;
		while (true) {
			if (p.EndReached()) {
				return CodeError::Bad("Expected 'in'", (--p)->srcPos);
			} else if (p->text == "in") {
				if (vars.empty()) {
					return CodeError::Bad("Expected a variable name", p->srcPos);
				} else {
					return CodeError::Good();
				}
			} else if (mustTerminate) {
				return CodeError::Bad("Expected 'in'", p->srcPos);
			} else if (p->type != Token::Type::Word) {
				return CodeError::Bad("Expected a variable name", p->srcPos);
			}
			vars.push_back(p->text);
			++p;

			if (!p.EndReached() && p->text == ",") {
				isTuple = true;
				++p;
			} else {
				mustTerminate = true;
			}
		}
	}

	static CodeError ParseFor(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		++p;
		out.type = Statement::Type::For;

		std::vector<std::string> vars;
		bool isTuple{};
		if (auto error = ParseForLoopVariableList(p, vars, isTuple)) {
			return error;
		}
		++p;

		if (!isTuple) {
			out.forLoop.assignTarget.type = AssignType::Direct;
			out.forLoop.assignTarget.direct = vars[0];
		} else {
			out.forLoop.assignTarget.type = AssignType::Pack;
			for (auto& var : vars) {
				AssignTarget elem{};
				elem.type = AssignType::Direct;
				elem.direct = std::move(var);
				out.forLoop.assignTarget.pack.push_back(std::move(elem));
			}
		}

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

	CodeError ParseParameterList(TokenIter& p, std::vector<Parameter>& out) {
		out.clear();
		Parameter::Type type = Parameter::Type::Named;
		while (true) {
			if (p.EndReached()) {
				return CodeError::Good();
			} else if (p->text == "*") {
				if (type == Parameter::Type::ListArgs) {
					return CodeError::Bad("Only 1 variadic arguments parameter is allowed", p->srcPos);
				} else if (type == Parameter::Type::Kwargs) {
					return CodeError::Bad("Keyword arguments parameter must appear last", p->srcPos);
				}
				type = Parameter::Type::ListArgs;
				++p;
			} else if (p->text == "**") {
				if (type == Parameter::Type::Kwargs) {
					return CodeError::Bad("Only 1 keyword arguments parameter is allowed", p->srcPos);
				}
				type = Parameter::Type::Kwargs;
				++p;
			} else if (p->type != Token::Type::Word) {
				return CodeError::Good();
			} else {
				if (type != Parameter::Type::Named) {
					return CodeError::Bad("Regular parameters must appear first", p->srcPos);
				}
			}

			if (p.EndReached()) {
				return CodeError::Bad("Expected a parameter name", (--p)->srcPos);
			} else if (p->type != Token::Type::Word) {
				return CodeError::Bad("Expected a parameter name", p->srcPos);
			}

			std::string parameterName = p->text;

			// Check for duplicate parameters
			if (std::find_if(out.begin(), out.end(), [&](const Parameter& p) {
				return p.name == parameterName;
				}) != out.end()) {
				return CodeError::Bad("Duplicate parameter name", p->srcPos);
			}
			++p;

			std::optional<Expression> defaultValue;
			if (p.EndReached()) {
				out.push_back(Parameter{ parameterName, std::nullopt, type });
				return CodeError::Good();
			} else if (p->text == "=") {
				// Default value
				if (type != Parameter::Type::Named) {
					return CodeError::Bad("Only regular parameters can have a default argument", p->srcPos);
				}
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

			out.push_back(Parameter{ std::move(parameterName), std::move(defaultValue), type });

			if (p.EndReached()) {
				return CodeError::Good();
			} else if (p->text != ",") {
				return CodeError::Good();
			}
			++p;
		}
	}

	std::unordered_set<std::string> GetReferencedVariables(const AssignTarget& target) {
		if (target.type == AssignType::Direct) {
			return { target.direct };
		} else {
			std::unordered_set<std::string> variables;
			for (const auto& child : target.pack)
				variables.merge(GetReferencedVariables(child));
			return variables;
		}
	}

	// Get a set of variables referenced by an expression
	std::unordered_set<std::string> GetReferencedVariables(const Expression& expr) {
		std::unordered_set<std::string> variables;
		if (expr.operation == Operation::Variable) {
			variables.insert(expr.variableName);
		} else {
			for (const auto& child : expr.children) {
				variables.merge(GetReferencedVariables(child));
			}
		}
		return variables;
	}

	// Get a set of variables directly written to by the '=' operator. This excludes compound assignment.
	static std::unordered_set<std::string> GetWriteVariables(const Expression& expr) {
		if (expr.operation == Operation::Assign && (expr.assignTarget.type == AssignType::Direct || expr.assignTarget.type == AssignType::Pack)) {
			return GetReferencedVariables(expr.assignTarget);
		} else {
			std::unordered_set<std::string> variables;
			for (const auto& child : expr.children)
				variables.merge(GetWriteVariables(child));
			return variables;
		}
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

		std::function<void(const std::vector<Statement>&)> scanNode = [&](const std::vector<Statement>& body) {
			for (const auto& child : body) {
				bool isFn = child.expr.operation == Operation::Function;
				switch (child.type) {
				case Statement::Type::Expr:
				case Statement::Type::If:
				case Statement::Type::Elif:
				case Statement::Type::While:
				case Statement::Type::Return:
					if (isFn) {
						writeVars.insert(child.expr.def.name);
						allVars.insert(child.expr.def.name);
						for (const auto& parameter : child.expr.def.parameters) {
							if (parameter.defaultValue) {
								writeVars.merge(GetWriteVariables(parameter.defaultValue.value()));
								allVars.merge(GetReferencedVariables(parameter.defaultValue.value()));
							}
						}
						allVars.insert(child.expr.def.localCaptures.begin(), child.expr.def.localCaptures.end());
					} else {
						writeVars.merge(GetWriteVariables(child.expr));
						allVars.merge(GetReferencedVariables(child.expr));
					}
					break;
				case Statement::Type::Class:
					writeVars.insert(child.klass.name);
					allVars.insert(child.klass.name);
					break;
				case Statement::Type::Def:
					writeVars.insert(child.expr.def.name);
					allVars.insert(child.expr.def.name);
					break;
				case Statement::Type::Global:
					defNode.expr.def.globalCaptures.insert(child.capture.name);
					break;
				case Statement::Type::Nonlocal:
					defNode.expr.def.localCaptures.insert(child.capture.name);
					break;
				}

				if (!isFn) {
					scanNode(child.body);
				}
			}
		};

		scanNode(defNode.expr.def.body);

		std::vector<std::string> parameterVars;
		for (const auto& param : defNode.expr.def.parameters)
			parameterVars.push_back(param.name);
		defNode.expr.def.localCaptures.merge(SetDifference(allVars, writeVars, parameterVars));
		defNode.expr.def.variables = SetDifference(writeVars, defNode.expr.def.globalCaptures, defNode.expr.def.localCaptures, parameterVars);
	}

	static CodeError ParseDef(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		out.type = Statement::Type::Def;
		++p;

		Expression fn{};
		fn.srcPos = node.tokens[0].srcPos;
		fn.operation = Operation::Function;

		if (p.EndReached()) {
			return CodeError::Bad("Expected a function name", (--p)->srcPos);
		} else if (p->type != Token::Type::Word) {
			return CodeError::Bad("Expected a function name", p->srcPos);
		}
		fn.def.name = p->text;
		++p;

		if (p.EndReached()) {
			return CodeError::Bad("Expected a '('", (--p)->srcPos);
		} else if (p->text != "(") {
			return CodeError::Bad("Expected a '('", p->srcPos);
		}
		++p;

		if (auto error = ParseParameterList(p, fn.def.parameters)) {
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

		if (auto error = ParseBody(node, Statement::Type::Def, fn.def.body)) {
			return error;
		}

		out.expr = std::move(fn);

		ResolveCaptures(out);

		return CodeError::Good();
	}

	static CodeError ParseClass(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		out.type = Statement::Type::Class;
		++p;

		if (p.EndReached()) {
			return CodeError::Bad("Expected a class name", (--p)->srcPos);
		} else if (p->type != Token::Type::Word) {
			return CodeError::Bad("Expected a class name", p->srcPos);
		}
		out.klass.name = p->text;
		++p;

		if (p.EndReached()) {
			return CodeError::Bad("Expected a ':'", (--p)->srcPos);
		} else if (p->text == "(") {
			++p;
			if (auto error = ParseExpressionList(p, ")", out.klass.bases)) {
				return error;
			}
			++p;
		}

		if (node.children.empty()) {
			return CodeError::Bad("Expected class body", (--p)->srcPos);
		}

		if (auto error = ExpectColonEnding(p)) {
			return error;
		}

		for (const auto& method : node.children) {
			if (method.tokens[0].text == "pass") {
				continue;
			} else if (method.tokens[0].text != "def") {
				return CodeError::Bad("Expected a method definition");
			}

			Statement stat{};
			if (auto error = ParseDef(method, stat)) {
				return error;
			}
			stat.srcPos = method.tokens[0].srcPos;
			out.klass.methodNames.push_back(stat.expr.def.name);
			out.body.push_back(std::move(stat));
		}

		return CodeError::Good();
	}

	static CodeError ParseTry(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		++p;

		if (auto error = ExpectColonEnding(p)) {
			return error;
		}

		out.type = Statement::Type::Try;
		return ParseBody(node, Statement::Type::Try, out.body);
	}

	static CodeError ParseExcept(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		++p;

		Expression exceptType{};
		if (p.EndReached()) {
			return CodeError::Bad("Expected a ':'", (--p)->srcPos);
		} else if (p->text == ":") {
			goto end;
		} else if (auto error = ParseExpression(p, exceptType)) {
			return error;
		}
		out.exceptBlock.exceptType = std::move(exceptType);

		if (p.EndReached()) {
			return CodeError::Bad("Expected a ':'", (--p)->srcPos);
		} else if (p->text == ":") {
			goto end;
		} else if (p->text != "as") {
			return CodeError::Bad("Expected a 'as'", p->srcPos);
		}
		++p;

		if (p.EndReached()) {
			return CodeError::Bad("Expected an identifier", (--p)->srcPos);
		} else if (p->type != Token::Type::Word) {
			return CodeError::Bad("Expected an identifier", p->srcPos);
		}
		out.exceptBlock.var = p->text;
		++p;
		
	end:
		if (auto error = ExpectColonEnding(p)) {
			return error;
		}

		out.type = Statement::Type::Except;
		return ParseBody(node, Statement::Type::Except, out.body);
	}

	static CodeError ParseFinally(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		++p;

		if (auto error = ExpectColonEnding(p)) {
			return error;
		}

		out.type = Statement::Type::Finally;
		return ParseBody(node, Statement::Type::Finally, out.body);
	}

	static CodeError ParseRaise(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		++p;

		out.type = Statement::Type::Raise;
		if (auto error = ParseExpression(p, out.expr)) {
			return error;
		} else {
			return CheckTrailingTokens(p);
		}
	}

	static CodeError ParseWith(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		SourcePosition srcPos = p->srcPos;
		++p;

		Expression manager{};
		if (p.EndReached()) {
			return CodeError::Bad("Expected a ':'", (--p)->srcPos);
		} else if (auto error = ParseExpression(p, manager)) {
			return error;
		}

		std::string var;
		if (p.EndReached()) {
			return CodeError::Bad("Expected a ':'", (--p)->srcPos);
		} else if (p->text == ":") {
			goto end;
		} else if (p->text != "as") {
			return CodeError::Bad("Expected a 'as'", p->srcPos);
		}
		++p;

		if (p.EndReached()) {
			return CodeError::Bad("Expected an identifier", (--p)->srcPos);
		} else if (p->type != Token::Type::Word) {
			return CodeError::Bad("Expected an identifier", p->srcPos);
		}
		var = p->text;
		++p;

	end:
		if (auto error = ExpectColonEnding(p)) {
			return error;
		}

		std::vector<Statement> body;
		if (auto error = ParseBody(node, Statement::Type::Composite, body)) {
			return error;
		}

		/*
		 * __WithMgr = <expr>
		 * [<var> =] __WithMgr.__enter__()
		 * try:
		 *		<body>
		 * finally:
		 * 		__WithMgr.__exit__(None, None, None)
		 */

		std::vector<Statement> mainBody;

		// __WithMgr = <expr>
		std::string mgrName = "__WithMgr" + std::to_string(Guid());
		Expression assignMgr{};
		assignMgr.srcPos = srcPos;
		assignMgr.operation = Operation::Assign;
		assignMgr.assignTarget.type = AssignType::Direct;
		assignMgr.assignTarget.direct = mgrName;
		assignMgr.children.push_back({}); // Dummy
		assignMgr.children.push_back(std::move(manager));

		Statement assignMgrStat{};
		assignMgrStat.srcPos = srcPos;
		assignMgrStat.type = Statement::Type::Expr;
		assignMgrStat.expr = std::move(assignMgr);
		mainBody.push_back(std::move(assignMgrStat));

		// [<var> =] __WithMgr.__enter__()
		auto loadMgr = [&] {
			Expression load{};
			load.srcPos = srcPos;
			load.operation = Operation::Variable;
			load.variableName = mgrName;
			return load;
		};

		Expression enter{};
		enter.srcPos = srcPos;
		enter.operation = Operation::Dot;
		enter.variableName = "__enter__";
		enter.children.push_back(loadMgr());

		Expression enterCall{};
		enterCall.srcPos = srcPos;
		enterCall.operation = Operation::Call;
		enterCall.children.push_back(std::move(enter));

		Statement enterStat{};
		enterStat.srcPos = srcPos;
		enterStat.type = Statement::Type::Expr;
		if (!var.empty()) {
			Expression assign{};
			assign.srcPos = srcPos;
			assign.operation = Operation::Assign;
			assign.assignTarget.type = AssignType::Direct;
			assign.assignTarget.direct = std::move(var);
			assign.children.push_back({}); // Dummy
			assign.children.push_back(std::move(enterCall));
			enterStat.expr = std::move(assign);
		} else {
			enterStat.expr = std::move(enterCall);
		}
		mainBody.push_back(std::move(enterStat));

		// __WithMgr.__exit__(None, None, None)
		Expression loadExit{};
		loadExit.srcPos = srcPos;
		loadExit.operation = Operation::Dot;
		loadExit.variableName = "__exit__";
		loadExit.children.push_back(loadMgr());

		auto loadNone = [&] {
			Expression none{};
			none.srcPos = srcPos;
			none.operation = Operation::Literal;
			none.literalValue.type = LiteralValue::Type::Null;
			return none;
		};
		
		Expression exit{};
		exit.srcPos = srcPos;
		exit.operation = Operation::Call;
		exit.children.push_back(std::move(loadExit));
		exit.children.push_back(loadNone());
		exit.children.push_back(loadNone());
		exit.children.push_back(loadNone());

		Statement exitStat{};
		exitStat.srcPos = srcPos;
		exitStat.type = Statement::Type::Expr;
		exitStat.expr = std::move(exit);

		// try/finally
		Statement tryBlock{};
		tryBlock.srcPos = srcPos;
		tryBlock.type = Statement::Type::Try;
		tryBlock.body = std::move(body);
		tryBlock.tryBlock.finallyClause.push_back(std::move(exitStat));
		mainBody.push_back(std::move(tryBlock));

		// Produce composite statement
		out.type = Statement::Type::Composite;
		out.body = std::move(mainBody);
		return CodeError::Good();
	}

	static CodeError CheckBreakable(const LexTree& node) {
		auto it = statementHierarchy.rbegin();
		while (true) {
			switch (*it) {
			case Statement::Type::Def:
			case Statement::Type::Root:
				return CodeError::Bad("'break' or 'continue' outside of loop", node.tokens[0].srcPos);
			case Statement::Type::For:
			case Statement::Type::While:
				return CodeError::Good();
			}
			++it;
		}
	}

	static size_t BreakableTryExceptCount() {
		size_t count = 0;
		auto it = statementHierarchy.rbegin();
		while (true) {
			switch (*it) {
			case Statement::Type::Def:
			case Statement::Type::Root:
			case Statement::Type::For:
			case Statement::Type::While:
				return count;
			case Statement::Type::Try:
				count++;
				break;
			}
			++it;
		}
	}

	static size_t TotalTryExceptCount() {
		size_t count = 0;
		auto it = statementHierarchy.rbegin();
		while (true) {
			switch (*it) {
			case Statement::Type::Def:
			case Statement::Type::Root:
				return count;
			case Statement::Type::Try:
				count++;
				break;
			}
			++it;
		}
	}

	static CodeError ParseReturn(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		++p;

		out.type = Statement::Type::Return;
		if (p.EndReached()) {
			out.expr.operation = Operation::Literal;
			out.expr.literalValue.type = LiteralValue::Type::Null;
			out.expr.srcPos = (--p)->srcPos;
			out.exitBlock.finallyCount = TotalTryExceptCount();
			return CodeError::Good();
		} else if (auto error = ParseExpression(p, out.expr)) {
			return error;
		} else {
			out.exitBlock.finallyCount = TotalTryExceptCount();
			return CheckTrailingTokens(p);
		}
	}

	static CodeError ParseSingleToken(const LexTree& node, Statement& out, Statement::Type type) {
		TokenIter p(node.tokens);
		++p;
		out.type = type;
		return CheckTrailingTokens(p);
	}

	static CodeError ParseBreak(const LexTree& node, Statement& out) {
		if (auto error = CheckBreakable(node)) {
			return error;
		} else if (auto error = ParseSingleToken(node, out, Statement::Type::Break)) {
			return error;
		}
		out.exitBlock.finallyCount = BreakableTryExceptCount();
		return CodeError::Good();
	}

	static CodeError ParseContinue(const LexTree& node, Statement& out) {
		if (auto error = CheckBreakable(node)) {
			return error;
		} else if (auto error = ParseSingleToken(node, out, Statement::Type::Continue)) {
			return error;
		}
		out.exitBlock.finallyCount = BreakableTryExceptCount();
		return CodeError::Good();
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

	static CodeError ParseImportFrom(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		out.type = Statement::Type::ImportFrom;
		++p;

		if (p.EndReached()) {
			return CodeError::Bad("Expected a module name", (--p)->srcPos);
		} else if (p->type != Token::Type::Word) {
			return CodeError::Bad("Expected a module name", p->srcPos);
		}

		out.importFrom.module = p->text;
		++p;

		if (p.EndReached()) {
			return CodeError::Bad("Expected 'import'", (--p)->srcPos);
		} else if (p->text != "import") {
			return CodeError::Bad("Expected 'import'", p->srcPos);
		}
		++p;

		if (p.EndReached()) {
			return CodeError::Bad("Expected a name", (--p)->srcPos);
		}
		
		if (p->text == "*") {
			++p;
		} else {
			while (true) {
				if (p->type != Token::Type::Word) {
					return CodeError::Bad("Expected a name", p->srcPos);
				}
				out.importFrom.names.push_back(p->text);
				++p;

				if (p.EndReached()) {
					break;
				}

				if (p->text == "as") {
					++p;
					if (p.EndReached()) {
						return CodeError::Bad("Expected a name", (--p)->srcPos);
					} else if (p->type != Token::Type::Word) {
						return CodeError::Bad("Expected a name", p->srcPos);
					}
					out.importFrom.alias = p->text;
					++p;
					break;
				}
				
				if (p->text == ",") {
					++p;
				} else {
					return CodeError::Bad("Expected ','", p->srcPos);
				}
			}
		}

		return CheckTrailingTokens(p);
	}

	static CodeError ParseImport(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		out.type = Statement::Type::Import;
		++p;
		
		if (p.EndReached()) {
			return CodeError::Bad("Expected a module name", (--p)->srcPos);
		} else if (p->type != Token::Type::Word) {
			return CodeError::Bad("Expected a module name", p->srcPos);
		}

		out.import.module = p->text;
		++p;

		if (!p.EndReached() && p->text == "as") {
			++p;
			if (p.EndReached()) {
				return CodeError::Bad("Expected an alias name", (--p)->srcPos);
			} else if (p->type != Token::Type::Word) {
				return CodeError::Bad("Expected an alias name", p->srcPos);
			}
			out.import.alias = p->text;
			++p;
		}
		
		return CheckTrailingTokens(p);
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
		{ "class", ParseClass },
		{ "return", ParseReturn },
		{ "pass", ParsePass },
		{ "nonlocal", ParseNonlocal },
		{ "global", ParseGlobal },
		{ "try", ParseTry },
		{ "except", ParseExcept },
		{ "finally", ParseFinally },
		{ "raise", ParseRaise },
		{ "with", ParseWith },
		{ "from", ParseImportFrom },
		{ "import", ParseImport },
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

		out.srcPos = node.tokens[0].srcPos;
		return CodeError::Good();
	}

	void ExpandCompositeStatements(std::vector<Statement>& statements) {
		for (size_t i = 0; i < statements.size(); i++) {
			if (statements[i].type == Statement::Type::Composite) {
				for (size_t j = 0; j < statements[i].body.size(); j++) {
					auto& child = statements[i].body[j];
					statements.insert(statements.begin() + i + j + 1, std::move(child));
				}
				statements.erase(statements.begin() + i);
			}
		}
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

		ExpandCompositeStatements(out);

		// Validate elif and else
		for (size_t i = 0; i < out.size(); i++) {
			auto& stat = out[i];
			Statement::Type lastType = i ? out[i - 1].type : Statement::Type::Pass;

			if (stat.type == Statement::Type::Elif) {
				if (lastType != Statement::Type::If && lastType != Statement::Type::Elif) {
					return CodeError::Bad(
						"An 'elif' clause may only appear after an 'if' or 'elif' clause",
						stat.srcPos
					);
				}
			} else if (stat.type == Statement::Type::Else) {
				if (lastType != Statement::Type::If && lastType != Statement::Type::Elif && lastType != Statement::Type::While) {
					return CodeError::Bad(
						"An 'else' clause may only appear after an 'if', 'elif', 'while', or 'for' clause",
						stat.srcPos
					);
				}
			}
		}

		// Rearrange elif and else nodes
		for (size_t i = 0; i < out.size(); i++) {
			auto& stat = out[i];

			std::optional<Statement> elseClause;
			if (stat.type == Statement::Type::Elif) {
				// Transform elif into an else and if statement
				stat.type = Statement::Type::If;
				elseClause = Statement{};
				elseClause.value().srcPos = stat.srcPos;
				elseClause.value().type = Statement::Type::Else;
				elseClause.value().body.push_back(std::move(stat));
				out.erase(out.begin() + i);
				i--;

			} else if (stat.type == Statement::Type::Else) {
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

		for (size_t i = 0; i < out.size(); i++) {
			SourcePosition srcPos = out[i].srcPos;
			switch (out[i].type) {
			case Statement::Type::Except:
				return CodeError::Bad("An 'except' clause may only appear after a 'try' or 'except' clause", srcPos);
			case Statement::Type::Finally:
				return CodeError::Bad("A 'finally' clause may only appear after a 'try' or 'except' clause", srcPos);
			case Statement::Type::Try: {
				auto& tryStat = out[i];
				
				for (i++; i < out.size(); i++) {
					srcPos = out[i].srcPos;
					switch (out[i].type) {
					case Statement::Type::Except: {
						auto& exceptClauses = tryStat.tryBlock.exceptClauses;
						if (!exceptClauses.empty() && !exceptClauses.back().exceptBlock.exceptType.has_value()) {
							return CodeError::Bad("Default 'except' clause must be last", srcPos);
						}
						exceptClauses.push_back(std::move(out[i]));
						out.erase(out.begin() + i);
						i--;
						break;
					}
					case Statement::Type::Finally:
						tryStat.tryBlock.finallyClause = std::move(out[i].body);
						out.erase(out.begin() + i);
						i--;
						goto end;
					default:
						goto end;
					}
				}

			end:
				if (tryStat.tryBlock.exceptClauses.empty() && tryStat.tryBlock.finallyClause.empty()) {
					return CodeError::Bad("Expected an 'except' or 'finally' clause", srcPos);
				}
				i--;
			}
			}
		}

		return CodeError::Good();
	}

	ParseResult Parse(const LexTree& lexTree) {
		ParseResult result{};
		result.parseTree.type = Statement::Type::Root;

		if (lexTree.children.empty()) {
			return result;
		}

		statementHierarchy.clear();
		result.error = ParseBody(lexTree, Statement::Type::Root, result.parseTree.expr.def.body);
		statementHierarchy.clear();

		ResolveCaptures(result.parseTree);
		result.parseTree.expr.def.variables.merge(result.parseTree.expr.def.localCaptures);
		result.parseTree.expr.def.localCaptures.clear();

		return result;
	}
}
