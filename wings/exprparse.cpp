#include "exprparse.h"
#include "parse.h"
#include "impl.h"
#include <unordered_map>
#include <unordered_set>
#include <optional>

namespace wings {

	static thread_local bool disableInOperator;

	static CodeError ParseExpression(TokenIter& p, Expression& out, size_t minPrecedence, std::optional<Expression> preParsedArg = std::nullopt);

	TokenIter::TokenIter(const std::vector<Token>& tokens) :
		tokens(&tokens),
		index(0)
	{
	}

	TokenIter& TokenIter::operator++() {
		index++;
		return *this;
	}

	TokenIter& TokenIter::operator--() {
		index--;
		return *this;
	}

	const Token& TokenIter::operator*() const {
		return (*tokens)[index];
	}

	const Token* TokenIter::operator->() const {
		return &(*tokens)[index];
	}

	bool TokenIter::operator==(const TokenIter& rhs) const {
		return index == rhs.index && tokens == rhs.tokens;
	}

	bool TokenIter::operator!=(const TokenIter& rhs) const {
		return !(*this == rhs);
	}

	bool TokenIter::EndReached() const {
		return index >= tokens->size();
	}

	const std::vector<std::string> RESERVED = {
		"def", "if", "while", "for", "in", "return",
		"True", "False", "None",
		"break", "continue", "pass", "else", "elif",
		"or", "and", "not", "global", "nonlocal",
	};

	const std::unordered_map<std::string, Operation> BINARY_OP_STRINGS = {
		{ "+",  Operation::Add },
		{ "-",  Operation::Sub },
		{ "*",  Operation::Mul },
		{ "**", Operation::Pow },
		{ "/",  Operation::Div },
		{ "//", Operation::IDiv },
		{ "%",  Operation::Mod },
		{ "<",  Operation::Lt },
		{ ">",  Operation::Gt },
		{ "<=", Operation::Le },
		{ ">=", Operation::Ge },
		{ "==", Operation::Eq },
		{ "!=", Operation::Ne },
		{ "and", Operation::And },
		{ "or", Operation::Or },
		{ "^",  Operation::BitXor },
		{ "&",  Operation::BitAnd },
		{ "|",  Operation::BitOr },
		{ "<<", Operation::ShiftL },
		{ ">>", Operation::ShiftR },
		{ "in", Operation::In },
		{ "not", Operation::NotIn },

		{ "=",  Operation::Assign },
		{ ":=",  Operation::Assign },
		{ "+=", Operation::AddAssign },
		{ "-=", Operation::SubAssign },
		{ "*=", Operation::MulAssign },
		{ "**=", Operation::PowAssign },
		{ "/=", Operation::DivAssign },
		{ "//=", Operation::IDivAssign },
		{ "%=", Operation::ModAssign },
		{ "<<=", Operation::ShiftLAssign },
		{ ">>=", Operation::ShiftRAssign },
		{ "|=", Operation::OrAssign },
		{ "&=", Operation::AndAssign },
		{ "^=", Operation::XorAssign },
		{ ".", Operation::Dot },
	};

	const std::unordered_map<std::string, Operation> PREFIX_UNARY_OP_STRINGS = {
		{ "+", Operation::Pos },
		{ "-", Operation::Neg },
		{ "~", Operation::BitNot },
		{ "not", Operation::Not },
	};

	const std::unordered_set<Operation> BINARY_OPS = {
		Operation::Add,
		Operation::Sub,
		Operation::Mul,
		Operation::Pow,
		Operation::Div,
		Operation::IDiv,
		Operation::Mod,
		Operation::Lt,
		Operation::Gt,
		Operation::Le,
		Operation::Ge,
		Operation::Eq,
		Operation::Ne,
		Operation::And,
		Operation::Or,
		Operation::BitXor,
		Operation::BitAnd,
		Operation::BitOr,
		Operation::ShiftL,
		Operation::ShiftR,
		Operation::In,
		Operation::NotIn,
		Operation::Dot,

		Operation::Assign,
		Operation::AddAssign,
		Operation::SubAssign,
		Operation::MulAssign,
		Operation::PowAssign,
		Operation::DivAssign,
		Operation::IDivAssign,
		Operation::ModAssign,
		Operation::ShiftLAssign,
		Operation::ShiftRAssign,
		Operation::OrAssign,
		Operation::AndAssign,
		Operation::XorAssign,
	};

	const std::unordered_set<Operation> BINARY_RIGHT_ASSOCIATIVE_OPS = {
		Operation::Assign,
		Operation::AddAssign,
		Operation::SubAssign,
		Operation::MulAssign,
		Operation::PowAssign,
		Operation::DivAssign,
		Operation::IDivAssign,
		Operation::ModAssign,
		Operation::ShiftLAssign,
		Operation::ShiftRAssign,
		Operation::OrAssign,
		Operation::AndAssign,
		Operation::XorAssign,
	};

	const std::unordered_set<Operation> PREFIX_UNARY_OPS = {
		Operation::Pos,
		Operation::Neg,
		Operation::Not,
		Operation::BitNot,
		Operation::Incr,
		Operation::Decr,
	};

	const std::vector<std::vector<Operation>> PRECEDENCE = {
		{ Operation::Call, Operation::Index, Operation::Slice, Operation::Incr, Operation::Decr, Operation::Dot },
		{ Operation::Pow },
		{ Operation::Pos, Operation::Neg, Operation::BitNot },
		{ Operation::Mul, Operation::Div, Operation::IDiv, Operation::Mod },
		{ Operation::Add, Operation::Sub },
		{ Operation::ShiftL, Operation::ShiftR },
		{ Operation::BitAnd },
		{ Operation::BitXor },
		{ Operation::BitOr },
		{
			Operation::Eq, Operation::Ne, Operation::Lt, Operation::Le, Operation::Gt,
			Operation::Ge, Operation::In, Operation::NotIn
		},
		{ Operation::Not },
		{ Operation::And },
		{ Operation::Or },
		{ Operation::IfElse },
		{
			Operation::Assign, Operation::AddAssign, Operation::SubAssign,
			Operation::MulAssign, Operation::DivAssign, Operation::IDivAssign,
			Operation::ModAssign, Operation::ShiftLAssign, Operation::ShiftRAssign,
			Operation::AndAssign, Operation::OrAssign, Operation::XorAssign, Operation::PowAssign,
		},
	};

	static size_t PrecedenceOf(Operation op) {
		auto it = std::find_if(
			PRECEDENCE.begin(),
			PRECEDENCE.end(),
			[=](const auto& group) { return std::find(group.begin(), group.end(), op) != group.end(); }
		);
		return std::distance(it, PRECEDENCE.end());
	}

	bool IsAssignableExpression(const Expression& expr, AssignTarget& target, bool onlyDirectOrPack) {
		target.type = AssignType::None;
		switch (expr.operation) {
		case Operation::Variable:
			target.type = AssignType::Direct;
			target.direct = expr.variableName;
			return true;
		case Operation::Index:
		case Operation::Slice:
			if (onlyDirectOrPack)
				return false;
			target.type = AssignType::Index;
			return true;
		case Operation::Dot:
			if (onlyDirectOrPack)
				return false;
			target.type = AssignType::Member;
			return true;
		case Operation::Tuple:
		case Operation::List:
			for (const auto& child : expr.children)
				if (!IsAssignableExpression(child, target.pack.emplace_back(), true))
					return false;
			target.type = AssignType::Pack;
			return true;
		default:
			return false;
		}
	}

	static CodeError ParseExpressionList(TokenIter& p, const std::string& terminate, std::vector<Expression>& out, bool* seenComma = nullptr) {
		bool mustTerminate = false;
		if (seenComma) *seenComma = false;
		while (true) {
			// Check for terminating token
			if (p.EndReached()) {
				return CodeError::Bad("Expected a closing bracket", (--p)->srcPos);
			} else if (p->text == terminate) {
				return CodeError::Good();
			} else if (mustTerminate) {
				return CodeError::Bad("Expected a closing bracket", p->srcPos);
			}

			// Get expression
			Expression expr{};
			if (auto error = ParseExpression(p, expr)) {
				return error;
			}
			out.push_back(std::move(expr));

			// Check for comma
			if (!p.EndReached() && p->text == ",") {
				if (seenComma) *seenComma = true;
				++p;
			} else {
				mustTerminate = true;
			}
		}
	}

	static CodeError ParsePostfix(TokenIter& p, Expression arg, Expression& out) {
		if (p.EndReached()) {
			out = std::move(arg);
			return CodeError::Good();
		}

		out.srcPos = p->srcPos;
		if (p->text == "++" || p->text == "--") {
			if (!IsAssignableExpression(arg, out.assignTarget)) {
				return CodeError::Bad("Expression is not assignable", (--p)->srcPos);
			}

			out.operation = p->text == "++" ? Operation::Incr : Operation::Decr;
			out.children.push_back(std::move(arg));
			++p;
		} else if (p->text == "(") {
			// Consume opening bracket
			out.operation = Operation::Call;
			++p;

			// Consume expression list
			out.children.push_back(std::move(arg));
			if (p.EndReached()) {
				return CodeError::Bad("Expected an expression", (--p)->srcPos);
			} else if (auto error = ParseExpressionList(p, ")", out.children)) {
				return error;
			}

			// Consume closing bracket
			++p;
		} else if (p->text == "[") {
			// Consume opening bracket
			SourcePosition srcPos = p->srcPos;
			++p;

			std::optional<Expression> indices[3];

			bool isSlice = false;
			for (size_t i = 0; i < std::size(indices); i++) {
				if (p.EndReached()) {
					return CodeError::Bad("Expected an expression", (--p)->srcPos);
				} else if (p->text != ":") {
					indices[i].emplace();
					if (auto error = ParseExpression(p, indices[i].value())) {
						return error;
					}
				}

				// Consume ']' or ':'
				if (p.EndReached()) {
					return CodeError::Bad("Expected a ']'", (--p)->srcPos);
				} else if (p->text == "]") {
					++p;
					break;
				} else if (p->text != ":") {
					return CodeError::Bad("Expected a ']'", p->srcPos);
				}
				isSlice = true;
				++p;
			}

			out.operation = isSlice ? Operation::Slice : Operation::Index;
			out.children.push_back(std::move(arg));
			for (size_t i = 0; i < std::size(indices); i++) {
				if (indices[i].has_value()) {
					out.children.push_back(std::move(indices[i].value()));
				} else if (isSlice) {
					Expression none{};
					none.srcPos = srcPos;
					none.literalValue.type = LiteralValue::Type::Null;
					out.children.push_back(std::move(none));
				}
			}
		} else if (p->text == ".") {
			// Consume dot
			out.operation = Operation::Dot;
			++p;

			// Consume attribute name
			if (p.EndReached()) {
				return CodeError::Bad("Expected an attribute name", (--p)->srcPos);
			} else if (p->type != Token::Type::Word) {
				return CodeError::Bad("Expected an attribute name", p->srcPos);
			}
			out.children.push_back(std::move(arg));
			out.variableName = p->text;
			++p;
		} else if (p->text == "if") {
			// Might as well handle if-else here
			out.operation = Operation::IfElse;

			// Consume 'if'
			++p;

			// Consume condition
			Expression condition;
			if (p.EndReached()) {
				return CodeError::Bad("Expected an expression", (--p)->srcPos);
			} else if (auto error = ParseExpression(p, condition, (size_t)0)) {
				return error;
			}

			// Consume 'else'
			if (p.EndReached()) {
				return CodeError::Bad("Expected 'else'", (--p)->srcPos);
			} else if (p->text != "else") {
				return CodeError::Bad("Expected 'else'", p->srcPos);
			}
			++p;

			// Consume false case expression
			Expression falseCase;
			if (p.EndReached()) {
				return CodeError::Bad("Expected an expression", (--p)->srcPos);
			} else if (auto error = ParseExpression(p, falseCase, (size_t)0)) {
				return error;
			}

			out.children.push_back(std::move(condition));
			out.children.push_back(std::move(arg));
			out.children.push_back(std::move(falseCase));
		} else {
			out = std::move(arg);
		}
		return CodeError::Good();
	}

	static CodeError ParseTuple(TokenIter& p, Expression& out) {
		out.srcPos = p->srcPos;
		out.operation = Operation::Tuple;
		++p;

		bool seenComma = false;
		if (p.EndReached()) {
			return CodeError::Bad("Expected an expression", (--p)->srcPos);
		} else if (auto error = ParseExpressionList(p, ")", out.children, &seenComma)) {
			return error;
		}
		++p;

		if (!out.children.empty() && !seenComma) {
			// Was just an expression in brackets and not a tuple.
			Expression e = std::move(out.children[0]);
			out = std::move(e);
		}

		return CodeError::Good();
	}

	static CodeError ParseListLiteral(TokenIter& p, Expression& out) {
		out.srcPos = p->srcPos;
		out.operation = Operation::List;
		++p;

		if (p.EndReached()) {
			return CodeError::Bad("Expected an expression", (--p)->srcPos);
		} else if (auto error = ParseExpressionList(p, "]", out.children)) {
			return error;
		}

		++p;

		return CodeError::Good();
	}

	static CodeError ParseMapLiteral(TokenIter& p, Expression& out) {
		out.srcPos = p->srcPos;
		out.operation = Operation::Map;
		++p;
		bool mustTerminate = false;
		while (true) {
			// Check for terminating token
			if (p.EndReached()) {
				return CodeError::Bad("Expected a closing bracket", (--p)->srcPos);
			} else if (p->text == "}") {
				++p;
				return CodeError::Good();
			} else if (mustTerminate) {
				return CodeError::Bad("Expected a closing bracket", p->srcPos);
			}

			// Get key
			Expression key{};
			if (auto error = ParseExpression(p, key)) {
				return error;
			}
			out.children.push_back(std::move(key));

			// Check for colon
			if (p.EndReached()) {
				return CodeError::Bad("Expected a ':'", (--p)->srcPos);
			} else if (p->text != ":") {
				return CodeError::Bad("Expected a ':'", p->srcPos);
			}
			++p;

			// Get value
			Expression value{};
			if (auto error = ParseExpression(p, value)) {
				return error;
			}
			out.children.push_back(std::move(value));

			// Check for comma
			if (!p.EndReached() && p->text == ",") {
				++p;
			} else {
				mustTerminate = true;
			}
		}
	}

	static CodeError TryParseListComprehension(TokenIter& p, Expression& out, bool& isListComp) {
		isListComp = false;
		out.srcPos = p->srcPos;
		out.operation = Operation::ListComprehension;
		TokenIter begin = p;
		++p;

		Expression value{};
		if (auto error = ParseExpression(p, value)) {
			p = begin;
			return CodeError::Good();
		}

		if (p.EndReached()) {
			p = begin;
			return CodeError::Good();
		} else if (p->text != "for") {
			p = begin;
			return CodeError::Good();
		}
		isListComp = true;
		++p;

		Expression var{};
		AssignTarget assignTarget{};
		if (p.EndReached()) {
			return CodeError::Bad("Expected a variable name", (--p)->srcPos);
		} else if (auto error = ParseExpression(p, var, true)) {
			return error;
		} else if (!IsAssignableExpression(var, assignTarget, true)) {
			return CodeError::Bad("Expression is not assignable", (--p)->srcPos);
		}

		if (p.EndReached()) {
			return CodeError::Bad("Expected a 'in'", (--p)->srcPos);
		} else if (p->text != "in") {
			return CodeError::Bad("Expected a 'in'", p->srcPos);
		}
		++p;

		Expression iterable{};
		if (auto error = ParseExpression(p, iterable)) {
			return error;
		}

		if (p.EndReached()) {
			return CodeError::Bad("Expected a ']'", (--p)->srcPos);
		} else if (p->text != "]") {
			return CodeError::Bad("Expected a ']'", p->srcPos);
		}
		++p;

		auto assignCaptures = GetReferencedVariables(var);
		Expression loadParam{};
		loadParam.srcPos = var.srcPos;
		loadParam.operation = Operation::Variable;
		loadParam.variableName = "_Arg";
		Expression assignExpr{};
		assignExpr.srcPos = var.srcPos;
		assignExpr.operation = Operation::Assign;
		assignExpr.assignTarget = assignTarget;
		assignExpr.children.push_back(std::move(var));
		assignExpr.children.push_back(std::move(loadParam));
		Statement assignStat{};
		assignStat.srcPos = var.srcPos;
		assignStat.type = Statement::Type::Expr;
		assignStat.expr = std::move(assignExpr);
		Expression assignFn{};
		assignFn.srcPos = var.srcPos;
		assignFn.operation = Operation::Function;
		assignFn.def.name = "<lambda>";
		assignFn.def.localCaptures = std::move(assignCaptures);
		assignFn.def.parameters.push_back(Parameter{ "_Arg", {} });
		assignFn.def.body.push_back(std::move(assignStat));

		auto exprCaptures = GetReferencedVariables(value);
		Statement exprRet{};
		exprRet.srcPos = value.srcPos;
		exprRet.type = Statement::Type::Return;
		exprRet.expr = std::move(value);
		Expression exprFn{};
		exprFn.srcPos = value.srcPos;
		exprFn.operation = Operation::Function;
		exprFn.def.name = "<lambda>";
		exprFn.def.localCaptures = std::move(exprCaptures);
		exprFn.def.body.push_back(std::move(exprRet));

		out.children.push_back(std::move(exprFn));
		out.children.push_back(std::move(assignFn));
		out.children.push_back(std::move(iterable));
		return CodeError::Good();
	}

	static CodeError ParseLambda(TokenIter& p, Expression& out) {
		out.srcPos = p->srcPos;
		++p;

		std::vector<Parameter> params;
		if (p.EndReached()) {
			return CodeError::Bad("Expected a ':'", (--p)->srcPos);
		} else if (auto error = ParseParameterList(p, params)) {
			return error;
		}

		if (p.EndReached()) {
			return CodeError::Bad("Expected a ':'", (--p)->srcPos);
		} else if (p->text != ":") {
			return CodeError::Bad("Expected a ':'", p->srcPos);
		}

		++p;
		Expression lambdaExpr{};
		if (auto error = ParseExpression(p, lambdaExpr)) {
			return error;
		}

		auto captures = GetReferencedVariables(lambdaExpr);
		for (const auto& param : params)
			captures.erase(param.name);

		Statement lambdaRet{};
		lambdaRet.srcPos = out.srcPos;
		lambdaRet.type = Statement::Type::Return;
		lambdaRet.expr = std::move(lambdaExpr);

		out.operation = Operation::Function;
		out.def.localCaptures = std::move(captures);
		out.def.name = "<lambda>";
		out.def.parameters = std::move(params);
		out.def.body.push_back(std::move(lambdaRet));

		return CodeError::Good();
	}

	static CodeError ParseValue(TokenIter& p, Expression& out) {
		// Parse standalone values
		out = {};
		if (p->text == "(") {
			if (auto error = ParseTuple(p, out)) {
				return error;
			}
		} else if (p->text == "[") {
			bool isListComprehension = false;
			if (auto error = TryParseListComprehension(p, out, isListComprehension)) {
				return error;
			} else if (isListComprehension) {
				// Do nothing
			} else if (auto error = ParseListLiteral(p, out)) {
				return error;
			}
		} else if (p->text == "{") {
			if (auto error = ParseMapLiteral(p, out)) {
				return error;
			}
		} else if (p->text == "lambda") {
			if (auto error = ParseLambda(p, out)) {
				return error;
			}
		} else {
			switch (p->type) {
			case Token::Type::Null:
				out.literalValue.type = LiteralValue::Type::Null;
				break;
			case Token::Type::Bool:
				out.literalValue.type = LiteralValue::Type::Bool;
				out.literalValue.b = p->literal.b;
				break;
			case Token::Type::Int:
				out.literalValue.type = LiteralValue::Type::Int;
				out.literalValue.i = p->literal.i;
				break;
			case Token::Type::Float:
				out.literalValue.type = LiteralValue::Type::Float;
				out.literalValue.f = p->literal.f;
				break;
			case Token::Type::String:
				out.literalValue.type = LiteralValue::Type::String;
				out.literalValue.s = p->literal.s;
				break;
			case Token::Type::Word:
				out.operation = Operation::Variable;
				out.variableName = p->text;
				break;
			default:
				return CodeError::Bad("Unexpected token", p->srcPos);
			}
			out.srcPos = p->srcPos;
			++p;
		}

		// Apply any postfix operators
		TokenIter oldP = p;
		do {
			Expression operand = std::move(out);
			out = {};
			oldP = p;
			if (auto error = ParsePostfix(p, std::move(operand), out)) {
				return error;
			}
		} while (oldP != p);

		return CodeError::Good();
	}

	static CodeError ParsePrefix(TokenIter& p, Expression& out) {
		if (PREFIX_UNARY_OP_STRINGS.contains(p->text)) {
			Operation op = PREFIX_UNARY_OP_STRINGS.at(p->text);
			out.srcPos = p->srcPos;
			++p;
			if (p.EndReached()) {
				return CodeError::Bad("Expected an expression", (--p)->srcPos);
			}
			out.operation = op;
			out.children.emplace_back();
			return ParsePrefix(p, out.children[0]);
		} else {
			return ParseValue(p, out);
		}
	}

	static CodeError ParseExpression(TokenIter& p, Expression& out, size_t minPrecedence, std::optional<Expression> preParsedArg) {
		Expression lhs{};
		if (preParsedArg.has_value()) {
			lhs = std::move(preParsedArg.value());
		} else {
			if (auto error = ParsePrefix(p, lhs)) {
				return error;
			}
		}

		if (p.EndReached() || !BINARY_OP_STRINGS.contains(p->text)) {
			out = std::move(lhs);
			return CodeError::Good();
		}
		Operation op = BINARY_OP_STRINGS.at(p->text);
		size_t precedence = PrecedenceOf(op);
		if (precedence < minPrecedence) {
			out = std::move(lhs);
			return CodeError::Good();
		} else if (op == Operation::NotIn) {
			// 'not in' is a special case since it contains 2 tokens
			out.srcPos = p->srcPos;
			++p;
			if (p.EndReached()) {
				return CodeError::Bad("Expected a 'in'", (--p)->srcPos);
			} else if (p->text != "in") {
				return CodeError::Bad("Expected a 'in'", p->srcPos);
			}
		} else if (disableInOperator && op == Operation::In) {
			out = std::move(lhs);
			return CodeError::Good();
		}
		++p;

		if (p.EndReached()) {
			return CodeError::Bad("Expected an expression", (--p)->srcPos);
		}
		out.srcPos = p->srcPos;
		if (BINARY_RIGHT_ASSOCIATIVE_OPS.contains(op)) {
			// Binary operation is an assignment operation only if it is right associative
			if (!IsAssignableExpression(lhs, out.assignTarget)) {
				return CodeError::Bad("Expression is not assignable", (----p)->srcPos);
			}

			Expression rhs{};
			if (auto error = ParseExpression(p, rhs)) {
				return error;
			}
			out.operation = op;
			out.children.push_back(std::move(lhs));
			out.children.push_back(std::move(rhs));
			return CodeError::Good();
		} else {
			Expression rhs{};
			if (auto error = ParseExpression(p, rhs, precedence + 1)) {
				return error;
			}
			out.operation = op;
			out.children.push_back(std::move(lhs));
			out.children.push_back(std::move(rhs));

			TokenIter oldP = p;
			do {
				lhs = std::move(out);
				out = {};
				oldP = p;
				if (auto error = ParseExpression(p, out, minPrecedence + 1, std::move(lhs))) {
					return error;
				}
			} while (oldP != p);
			return CodeError::Good();
		}
	}

	CodeError ParseExpression(TokenIter& p, Expression& out, bool disableInOp) {
		disableInOperator = disableInOp;
		if (p.EndReached()) {
			return CodeError::Bad("Expected an expression", (--p)->srcPos);
		} else {
			return ParseExpression(p, out, (size_t)0);
		}
	}
}
