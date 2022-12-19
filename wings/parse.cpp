#include "parse.h"

#include <unordered_map>
#include <functional>
#include <algorithm>
#include <iterator>

namespace wings {

	static thread_local std::vector<size_t> statementHierarchy;

	template <class StatType>
	static CodeError ParseBody(const LexTree& node, std::vector<Statement>& out);

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

	template <class StatType>
	static CodeError ParseConditionalBlock(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		++p;

		StatType data{};
		if (auto error = ParseExpression(p, data.expr)) {
			return error;
		}

		if (auto error = ExpectColonEnding(p)) {
			return error;
		}
		
		if (auto error = ParseBody<StatType>(node, data.body)) {
			return error;
		}
		
		out.data = std::move(data);
		return CodeError::Good();
	}

	static CodeError ParseIf(const LexTree& node, Statement& out) {
		return ParseConditionalBlock<stat::If>(node, out);
	}

	static CodeError ParseElif(const LexTree& node, Statement& out) {
		return ParseConditionalBlock<stat::Elif>(node, out);
	}

	static CodeError ParseElse(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		++p;
		
		if (auto error = ExpectColonEnding(p)) {
			return error;
		}

		stat::Else elseStat;
		if (auto error = ParseBody<stat::Else>(node, elseStat.body)) {
			return error;
		}

		out.data = std::move(elseStat);
		return CodeError::Good();
	}

	static CodeError ParseWhile(const LexTree& node, Statement& out) {
		return ParseConditionalBlock<stat::While>(node, out);
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

	Statement TransformForToWhile(stat::For forLoop) {
		// __VarXXX = expression.__iter__()
		std::string rangeVarName = "__For" + std::to_string(Guid());

		Expression loadIter;
		loadIter.srcPos = forLoop.expr.srcPos;
		loadIter.operation = Operation::Dot;
		loadIter.variableName = "__iter__";
		loadIter.children.push_back(std::move(forLoop.expr));

		Expression callIter;
		callIter.srcPos = forLoop.expr.srcPos;
		callIter.operation = Operation::Call;
		callIter.children.push_back(std::move(loadIter));
		
		Statement rangeEval;
		rangeEval.srcPos = forLoop.expr.srcPos;
		{
			stat::Expr assign;
			assign.expr.operation = Operation::Assign;
			assign.expr.srcPos = forLoop.expr.srcPos;
			assign.expr.assignTarget.type = AssignType::Direct;
			assign.expr.assignTarget.direct = rangeVarName;
			assign.expr.children.push_back({}); // Dummy
			assign.expr.children.push_back(std::move(callIter));
			rangeEval.data = std::move(assign);
		}

		// while True:
		Expression condition;
		condition.srcPos = forLoop.expr.srcPos;
		condition.operation = Operation::Literal;
		condition.literalValue.type = LiteralValue::Type::Bool;
		condition.literalValue.b = true;

		Statement wh;
		wh.srcPos = forLoop.expr.srcPos;
		{
			stat::While w;
			w.expr = std::move(condition);
			wh.data = std::move(w);
		}

		// try:
		//		__VarXXX = __VarXXX.__next__()
		// except StopIteration:
		//		break
		Statement brk;
		brk.srcPos = forLoop.expr.srcPos;
		{
			stat::Break b;
			b.finallyCount = 1;
			b.exitForLoopNormally = true;
			brk.data = std::move(b);
		}

		Expression stopIter;
		stopIter.srcPos = forLoop.expr.srcPos;
		stopIter.operation = Operation::Variable;
		stopIter.variableName = "StopIteration";

		Statement except;
		except.srcPos = forLoop.expr.srcPos;
		{
			stat::Except exc;
			exc.type = std::move(stopIter);
			exc.body.push_back(std::move(brk));
			except.data = std::move(exc);
		}

		Statement tryExcept;
		tryExcept.srcPos = forLoop.expr.srcPos;
		{
			stat::Try tr;
			tr.exceptBlocks.push_back(std::move(except));
			tryExcept.data = std::move(tr);
		}

		// vars = __VarXXX.__next__()
		Expression rangeVar;
		rangeVar.srcPos = forLoop.expr.srcPos;
		rangeVar.operation = Operation::Variable;
		rangeVar.variableName = rangeVarName;

		Expression loadNext;
		loadNext.srcPos = forLoop.expr.srcPos;
		loadNext.operation = Operation::Dot;
		loadNext.variableName = "__next__";
		loadNext.children.push_back(std::move(rangeVar));

		Expression callNext;
		callNext.srcPos = forLoop.expr.srcPos;
		callNext.operation = Operation::Call;
		callNext.children.push_back(std::move(loadNext));

		Expression iterAssign;
		iterAssign.srcPos = forLoop.expr.srcPos;
		iterAssign.operation = Operation::Assign;
		iterAssign.assignTarget = forLoop.assignTarget;
		iterAssign.children.push_back({}); // Dummy
		iterAssign.children.push_back(std::move(callNext));

		Statement iterAssignStat;
		iterAssignStat.srcPos = forLoop.expr.srcPos;
		{
			stat::Expr expr;
			expr.expr = std::move(iterAssign);
			iterAssignStat.data = std::move(expr);
		}
		tryExcept.Get<stat::Try>().body.push_back(std::move(iterAssignStat));

		// Transfer body over
		auto& whileBody = wh.Get<stat::While>().body;
		whileBody.push_back(std::move(tryExcept));
		for (auto& child : forLoop.body)
			whileBody.push_back(std::move(child));

		Statement out;
		out.srcPos = forLoop.expr.srcPos;
		{
			stat::Composite comp;
			comp.body.push_back(std::move(rangeEval));
			comp.body.push_back(std::move(wh));
			out.data = std::move(comp);
		}
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

		std::vector<std::string> vars;
		bool isTuple{};
		if (auto error = ParseForLoopVariableList(p, vars, isTuple)) {
			return error;
		}
		++p;

		stat::For forLoop;

		if (!isTuple) {
			forLoop.assignTarget.type = AssignType::Direct;
			forLoop.assignTarget.direct = vars[0];
		} else {
			forLoop.assignTarget.type = AssignType::Pack;
			for (auto& var : vars) {
				AssignTarget elem{};
				elem.type = AssignType::Direct;
				elem.direct = std::move(var);
				forLoop.assignTarget.pack.push_back(std::move(elem));
			}
		}

		if (auto error = ParseExpression(p, forLoop.expr)) {
			return error;
		}

		if (auto error = ExpectColonEnding(p)) {
			return error;
		}

		if (auto error = ParseBody<stat::For>(node, forLoop.body)) {
			return error;
		}

		out = TransformForToWhile(std::move(forLoop));
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

	static void ResolveCaptures(Expression& func) {
		std::unordered_set<std::string> writeVars;
		std::unordered_set<std::string> allVars;
		
		auto processExpression = [&]<class T>(const T* data) {
			const Expression& expr = data->expr;
			if (expr.operation == Operation::Function) {
				writeVars.insert(expr.def.name);
				allVars.insert(expr.def.name);
				for (const auto& parameter : expr.def.parameters) {
					if (parameter.defaultValue) {
						writeVars.merge(GetWriteVariables(parameter.defaultValue.value()));
						allVars.merge(GetReferencedVariables(parameter.defaultValue.value()));
					}
				}
				allVars.insert(expr.def.localCaptures.begin(), expr.def.localCaptures.end());
			} else {
				writeVars.merge(GetWriteVariables(expr));
				allVars.merge(GetReferencedVariables(expr));
			}
		};

		std::function<void(const std::vector<Statement>&)> scan =
		[&](const std::vector<Statement>& body) {
			for (const auto& child : body) {
				if (auto* node = child.GetIf<stat::Expr>()) {
					processExpression(node);
				} else if (auto* node = child.GetIf<stat::If>()) {
					processExpression(node);
					scan(node->body);
					if (node->elseClause)
						scan(node->elseClause->Get<stat::Else>().body);
				} else if (auto* node = child.GetIf<stat::Elif>()) {
					processExpression(node);
					scan(node->body);
				} else if (auto* node = child.GetIf<stat::While>()) {
					processExpression(node);
					scan(node->body);
				} else if (auto* node = child.GetIf<stat::Return>()) {
					processExpression(node);
				} else if (auto* node = child.GetIf<stat::Class>()) {
					writeVars.insert(node->name);
					allVars.insert(node->name);
				} else if (auto* node = child.GetIf<stat::Def>()) {
					writeVars.insert(node->expr.def.name);
					allVars.insert(node->expr.def.name);
				} else if (auto* node = child.GetIf<stat::Global>()) {
					func.def.globalCaptures.insert(node->name);
				} else if (auto* node = child.GetIf<stat::NonLocal>()) {
					func.def.localCaptures.insert(node->name);
				}
			}
		};

		scan(func.def.body);

		std::vector<std::string> parameterVars;
		for (const auto& param : func.def.parameters)
			parameterVars.push_back(param.name);
		func.def.localCaptures.merge(SetDifference(allVars, writeVars, parameterVars));
		func.def.variables = SetDifference(writeVars, func.def.globalCaptures, func.def.localCaptures, parameterVars);
	}

	static CodeError ParseDef(const LexTree& node, Statement& out) {		
		TokenIter p(node.tokens);
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

		if (auto error = ParseBody<stat::Def>(node, fn.def.body)) {
			return error;
		}
		
		ResolveCaptures(fn);
		
		stat::Def def;
		def.expr = std::move(fn);
		out.data = std::move(def);
		return CodeError::Good();
	}

	static CodeError ParseClass(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		++p;

		if (p.EndReached()) {
			return CodeError::Bad("Expected a class name", (--p)->srcPos);
		} else if (p->type != Token::Type::Word) {
			return CodeError::Bad("Expected a class name", p->srcPos);
		}
		stat::Class klass;
		klass.name = p->text;
		++p;

		if (p.EndReached()) {
			return CodeError::Bad("Expected a ':'", (--p)->srcPos);
		} else if (p->text == "(") {
			++p;
			if (auto error = ParseExpressionList(p, ")", klass.bases)) {
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

			Statement stat;
			if (auto error = ParseDef(method, stat)) {
				return error;
			}
			stat.srcPos = method.tokens[0].srcPos;
			klass.methodNames.push_back(stat.Get<stat::Def>().expr.def.name);
			klass.body.push_back(std::move(stat));
		}

		out.data = std::move(klass);
		return CodeError::Good();
	}

	static CodeError ParseTry(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		++p;

		if (auto error = ExpectColonEnding(p)) {
			return error;
		}
		
		stat::Try tr;
		if (auto error = ParseBody<stat::Try>(node, tr.body)) {
			return error;
		}

		out.data = std::move(tr);
		return CodeError::Good();
	}

	static CodeError ParseExcept(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		++p;

		stat::Except exc;

		Expression exceptType{};
		if (p.EndReached()) {
			return CodeError::Bad("Expected a ':'", (--p)->srcPos);
		} else if (p->text == ":") {
			goto end;
		} else if (auto error = ParseExpression(p, exceptType)) {
			return error;
		}
		exc.type = std::move(exceptType);

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
		exc.variable = p->text;
		++p;
		
	end:
		if (auto error = ExpectColonEnding(p)) {
			return error;
		}

		if (auto error = ParseBody<stat::Except>(node, exc.body)) {
			return error;
		}
		
		out.data = std::move(exc);
		return CodeError::Good();
	}

	static CodeError ParseFinally(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		++p;

		if (auto error = ExpectColonEnding(p)) {
			return error;
		}

		stat::Finally fin;
		if (auto error = ParseBody<stat::Finally>(node, fin.body)) {
			return error;
		}

		out.data = std::move(fin);
		return CodeError::Good();
	}

	static CodeError ParseRaise(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		++p;

		stat::Raise raise;
		if (auto error = ParseExpression(p, raise.expr)) {
			return error;
		}
		
		out.data = std::move(raise);
		return CheckTrailingTokens(p);
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
		if (auto error = ParseBody<stat::Composite>(node, body)) {
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

		Statement assignMgrStat;
		assignMgrStat.srcPos = srcPos;
		{
			stat::Expr expr;
			expr.expr = std::move(assignMgr);
			assignMgrStat.data = std::move(expr);
		}
		mainBody.push_back(std::move(assignMgrStat));

		// [<var> =] __WithMgr.__enter__()
		auto loadMgr = [&] {
			Expression load{};
			load.srcPos = srcPos;
			load.operation = Operation::Variable;
			load.variableName = mgrName;
			return load;
		};

		Expression enter;
		enter.srcPos = srcPos;
		enter.operation = Operation::Dot;
		enter.variableName = "__enter__";
		enter.children.push_back(loadMgr());

		Expression enterCall;
		enterCall.srcPos = srcPos;
		enterCall.operation = Operation::Call;
		enterCall.children.push_back(std::move(enter));

		Statement enterStat;
		enterStat.srcPos = srcPos;
		if (!var.empty()) {
			stat::Expr assign;
			assign.expr.srcPos = srcPos;
			assign.expr.operation = Operation::Assign;
			assign.expr.assignTarget.type = AssignType::Direct;
			assign.expr.assignTarget.direct = std::move(var);
			assign.expr.children.push_back({}); // Dummy
			assign.expr.children.push_back(std::move(enterCall));
			enterStat.data = std::move(assign);
		} else {
			stat::Expr expr;
			expr.expr = std::move(enterCall);
			enterStat.data = std::move(expr);
		}
		mainBody.push_back(std::move(enterStat));

		// __WithMgr.__exit__(None, None, None)
		Expression loadExit;
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
		
		Expression exit;
		exit.srcPos = srcPos;
		exit.operation = Operation::Call;
		exit.children.push_back(std::move(loadExit));
		exit.children.push_back(loadNone());
		exit.children.push_back(loadNone());
		exit.children.push_back(loadNone());

		Statement exitStat;
		exitStat.srcPos = srcPos;
		{
			stat::Expr expr;
			expr.expr = std::move(exit);
			exitStat.data = std::move(expr);
		}

		// try/finally
		Statement tryBlock{};
		tryBlock.srcPos = srcPos;
		{
			stat::Try tryStat;
			tryStat.body = std::move(body);
			tryStat.finallyBody.push_back(std::move(exitStat));
			tryBlock.data = std::move(tryStat);
		}
		mainBody.push_back(std::move(tryBlock));

		// Produce composite statement
		stat::Composite comp;
		comp.body = std::move(mainBody);
		out.data = std::move(comp);
		return CodeError::Good();
	}

	static CodeError CheckBreakable(const LexTree& node) {
		auto it = statementHierarchy.rbegin();
		while (true) {
			switch (*it) {
			case StatIndex<stat::Def>():
			case StatIndex<stat::Root>():
				return CodeError::Bad("'break' or 'continue' outside of loop", node.tokens[0].srcPos);
			case StatIndex<stat::For>():
			case StatIndex<stat::While>():
				return CodeError::Good();
			}
			++it;
		}
	}

	static size_t BreakableTryExceptCount() {
		size_t count = 0;
		for (auto it = statementHierarchy.rbegin(); ; ++it) {
			switch (*it) {
			case StatIndex<stat::Def>():
			case StatIndex<stat::Root>():
			case StatIndex<stat::For>():
			case StatIndex<stat::While>():
				return count;
			case StatIndex<stat::Try>():
				count++;
				break;
			}
		}
	}

	static size_t TotalTryExceptCount() {
		size_t count = 0;
		for (auto it = statementHierarchy.rbegin(); ; ++it) {
			switch (*it) {
			case StatIndex<stat::Def>():
			case StatIndex<stat::Root>():
				return count;
			case StatIndex<stat::Try>():
				count++;
				break;
			}
		}
	}
	
	static CodeError ParseReturn(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		++p;
		
		stat::Return expr;
		expr.finallyCount = TotalTryExceptCount();

		if (p.EndReached()) {
			expr.expr.operation = Operation::Literal;
			expr.expr.literalValue.type = LiteralValue::Type::Null;
			expr.expr.srcPos = (--p)->srcPos;
			out.data = std::move(expr);
			return CodeError::Good();
		} else if (auto error = ParseExpression(p, expr.expr)) {
			return error;
		} else {
			out.data = std::move(expr);
			return CheckTrailingTokens(p);
		}
	}

	static CodeError ValidateSingleToken(const LexTree& node) {
		TokenIter p(node.tokens);
		++p;
		return CheckTrailingTokens(p);
	}

	static CodeError ParseBreak(const LexTree& node, Statement& out) {
		if (auto error = CheckBreakable(node)) {
			return error;
		} else if (auto error = ValidateSingleToken(node)) {
			return error;
		}
		stat::Break brk;
		brk.finallyCount = BreakableTryExceptCount();
		out.data = std::move(brk);
		return CodeError::Good();
	}

	static CodeError ParseContinue(const LexTree& node, Statement& out) {
		if (auto error = CheckBreakable(node)) {
			return error;
		} else if (auto error = ValidateSingleToken(node)) {
			return error;
		}
		stat::Continue cont;
		cont.finallyCount = BreakableTryExceptCount();
		out.data = std::move(cont);
		return CodeError::Good();
	}

	static CodeError ParsePass(const LexTree& node, Statement& out) {
		out.data = stat::Pass();
		return ValidateSingleToken(node);
	}

	template <class StatType>
	static CodeError ParseCapture(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		++p;

		if (statementHierarchy.back() == StatIndex<stat::Root>()) {
			return CodeError::Bad("Cannot capture at top level", (--p)->srcPos);
		}

		if (p.EndReached()) {
			return CodeError::Bad("Expected a variable name", (--p)->srcPos);
		} else if (p->type != Token::Type::Word) {
			return CodeError::Bad("Expected a variable name", p->srcPos);
		}

		StatType stat{};
		stat.name = p->text;
		out.data = std::move(stat);
		++p;
		return CheckTrailingTokens(p);
	}

	static CodeError ParseNonlocal(const LexTree& node, Statement& out) {
		return ParseCapture<stat::NonLocal>(node, out);
	}

	static CodeError ParseGlobal(const LexTree& node, Statement& out) {
		return ParseCapture<stat::Global>(node, out);
	}

	static CodeError ParseExpressionStatement(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		stat::Expr expr;
		if (auto error = ParseExpression(p, expr.expr)) {
			return error;
		}
		out.data = std::move(expr);
		return CheckTrailingTokens(p);
	}

	static CodeError ParseImportFrom(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		++p;

		if (p.EndReached()) {
			return CodeError::Bad("Expected a module name", (--p)->srcPos);
		} else if (p->type != Token::Type::Word) {
			return CodeError::Bad("Expected a module name", p->srcPos);
		}

		stat::ImportFrom importFrom;
		importFrom.module = p->text;
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
				importFrom.names.push_back(p->text);
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
					importFrom.alias = p->text;
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

		out.data = std::move(importFrom);
		return CheckTrailingTokens(p);
	}

	static CodeError ParseImport(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		++p;
		
		if (p.EndReached()) {
			return CodeError::Bad("Expected a module name", (--p)->srcPos);
		} else if (p->type != Token::Type::Word) {
			return CodeError::Bad("Expected a module name", p->srcPos);
		}

		stat::Import import;
		import.module = p->text;
		++p;

		if (!p.EndReached() && p->text == "as") {
			++p;
			if (p.EndReached()) {
				return CodeError::Bad("Expected an alias name", (--p)->srcPos);
			} else if (p->type != Token::Type::Word) {
				return CodeError::Bad("Expected an alias name", p->srcPos);
			}
			import.alias = p->text;
			++p;
		}
		
		out.data = std::move(import);
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
		auto get = [&](size_t i) {
			return statements[i].GetIf<stat::Composite>();
		};
		
		for (size_t i = 0; i < statements.size(); i++) {
			if (auto* comp = get(i)) {
				for (size_t j = 0; j < get(i)->body.size(); j++) {
					auto& child = get(i)->body[j];
					statements.insert(statements.begin() + i + j + 1, std::move(child));
				}
				statements.erase(statements.begin() + i);
			}
		}
	}

	template <class StatType>
	static CodeError ParseBody(const LexTree& node, std::vector<Statement>& out) {
		out.clear();

		if (node.children.empty()) {
			return CodeError::Bad("Expected a statement", node.tokens.back().srcPos);
		}

		statementHierarchy.push_back(StatIndex<StatType>());
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
			size_t lastType = i ? out[i - 1].data.index() : 0;

			if (stat.Is<stat::Elif>()) {
				if (lastType != StatIndex<stat::If>() && lastType != StatIndex<stat::Elif>()) {
					return CodeError::Bad(
						"An 'elif' clause may only appear after an 'if' or 'elif' clause",
						stat.srcPos
					);
				}
			} else if (stat.Is<stat::Else>()) {
				if (lastType != StatIndex<stat::If>() && lastType != StatIndex<stat::Elif>() && lastType != StatIndex<stat::While>()) {
					return CodeError::Bad(
						"An 'else' clause may only appear after an 'if', 'elif', 'while', or 'for' clause",
						stat.srcPos
					);
				}
			}
		}

		// Rearrange elif and else nodes
		for (size_t i = 0; i < out.size(); i++) {
			auto& cur = out[i];

			std::optional<Statement> elseClause;
			if (auto* elif = cur.GetIf<stat::Elif>()) {
				// Transform elif into an else and if statement
				stat::If ifStat;
				ifStat.expr = std::move(elif->expr);
				ifStat.body = std::move(elif->body);
				ifStat.elseClause = std::move(elif->elseClause);
				cur.data = std::move(ifStat);
				
				SourcePosition srcPos = cur.srcPos;
				stat::Else elseStat;
				elseStat.body.push_back(std::move(cur));
				Statement stat;
				stat.srcPos = srcPos;
				stat.data = std::move(elseStat);
				elseClause = std::move(stat);
				out.erase(out.begin() + i);
				i--;
			} else if (cur.Is<stat::Else>()) {
				elseClause = std::move(cur);
				out.erase(out.begin() + i);
				i--;
			}

			if (elseClause) {
				auto getElse = [](Statement* parent) -> stat::Else* {
					std::unique_ptr<Statement>* els = nullptr;
					if (auto* ifStat = parent->GetIf<stat::If>()) {
						if (ifStat->elseClause)
							els = &ifStat->elseClause;
					} else if (auto* elif = parent->GetIf<stat::Elif>()) {
						if (elif->elseClause)
							els = &elif->elseClause;
					} else if (auto* wh = parent->GetIf<stat::While>()) {
						if (wh->elseClause)
							els = &wh->elseClause;
					} else {
						return nullptr;
					}
					return els ? &(*els)->Get<stat::Else>() : nullptr;
				};

				Statement* parent = &out[i];
				while (auto* els = getElse(parent)) {
					parent = &els->body.back();
				}
				
				auto els = std::make_unique<Statement>(std::move(elseClause.value()));
				if (auto* ifStat = parent->GetIf<stat::If>()) {
					ifStat->elseClause = std::move(els);
				} else if (auto* elif = parent->GetIf<stat::Elif>()) {
					elif->elseClause = std::move(els);
				} else if (auto* wh = parent->GetIf<stat::While>()) {
					wh->elseClause = std::move(els);
				} else {
					WG_UNREACHABLE();
				}
			}
		}

		for (size_t i = 0; i < out.size(); i++) {
			SourcePosition srcPos = out[i].srcPos;
			switch (out[i].data.index()) {
			case StatIndex<stat::Except>():
				return CodeError::Bad("An 'except' clause may only appear after a 'try' or 'except' clause", srcPos);
			case StatIndex<stat::Finally>():
				return CodeError::Bad("A 'finally' clause may only appear after a 'try' or 'except' clause", srcPos);
			case StatIndex<stat::Try>(): {
				auto& tryStat = out[i].Get<stat::Try>();
				
				for (i++; i < out.size(); i++) {
					auto& cur = out[i];
					srcPos = cur.srcPos;
					
					if (cur.Is<stat::Except>()) {
						auto& exceptClauses = tryStat.exceptBlocks;
						if (!exceptClauses.empty() && !exceptClauses.back().Get<stat::Except>().type) {
							return CodeError::Bad("Default 'except' clause must be last", srcPos);
						}
						exceptClauses.push_back(std::move(cur));
						out.erase(out.begin() + i);
						i--;
						continue;
					} else if (auto* fin = cur.GetIf<stat::Finally>()) {
						tryStat.finallyBody = std::move(fin->body);
						out.erase(out.begin() + i);
						i--;
					}
					break;
				}
				
				if (tryStat.exceptBlocks.empty() && tryStat.finallyBody.empty()) {
					return CodeError::Bad("Expected an 'except' or 'finally' clause", srcPos);
				}
				i--;
			}
			}
		}

		return CodeError::Good();
	}

	ParseResult Parse(const LexTree& lexTree) {
		if (lexTree.children.empty())
			return {};

		statementHierarchy.clear();
		stat::Root root;
		auto error = ParseBody<stat::Root>(lexTree, root.expr.def.body);
		statementHierarchy.clear();
		
		ResolveCaptures(root.expr);
		root.expr.def.variables.merge(root.expr.def.localCaptures);
		root.expr.def.localCaptures.clear();

		ParseResult result{};
		result.error = std::move(error);
		result.parseTree = std::move(root);
		return result;
	}
}
