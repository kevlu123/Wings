#include "exprparse.h"
#include "impl.h"
#include <unordered_map>
#include <unordered_set>

namespace wings {

	static CodeError ParseExpression(TokenIter& p, Expression& out, size_t minPrecedence);

	TokenIter::TokenIter(const std::vector<Token>& tokens) :
		tokens(tokens),
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
		return tokens[index];
	}

	const Token* TokenIter::operator->() const {
		return &tokens[index];
	}

	bool TokenIter::EndReached() const {
		return index >= tokens.size();
	}

	const std::vector<std::string> RESERVED = {
		"def", "if", "while", "for", "in", "return",
		"True", "False", "None",
		"break", "continue", "pass", "else", "elif",
		"or", "and", "not", "global", "nonlocal",
	};

	const std::unordered_map<std::string, Operation> OP_STRINGS = {
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

		{ "=",  Operation::Assign },
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

		{ "+", Operation::Pos },
		{ "-", Operation::Neg },
		{ "not", Operation::Not },
		{ "~", Operation::BitNot },
		{ "++", Operation::Incr },
		{ "--", Operation::Decr },
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
		{ Operation::Call, Operation::Index, Operation::Incr, Operation::Decr },
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

	static CodeError ParseExpressionList(TokenIter& p, const std::string& terminate, std::vector<Expression>& out) {
		bool mustTerminate = false;
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
				++p;
			} else {
				mustTerminate = true;
			}
		}
	}

	static CodeError ParsePostfix(TokenIter& p, Expression arg, Expression& out, bool& parsed) {
		if (p.EndReached()) {
			out = std::move(arg);
			parsed = false;
		} else if (p->text == "++" || p->text == "--") {
			out.operation = p->text == "++" ? Operation::Incr : Operation::Decr;
			out.children = { std::move(arg) };
			++p;
			parsed = true;
		} else if (p->text == "(" || p->text == "[") {
			// Consume opening bracket
			out.operation = p->text == "(" ? Operation::Call : Operation::Index;
			auto endBracket = p->text == "(" ? ")" : "]";
			++p;

			// Consume expression list
			out.children = { std::move(arg) };
			if (p.EndReached()) {
				return CodeError::Bad("Expected an expression", (--p)->srcPos);
			} else if (auto error = ParseExpressionList(p, endBracket, out.children)) {
				return error;
			}

			// Consume closing bracket
			++p;

			parsed = true;
		} else {
			out = std::move(arg);
			parsed = false;
		}
		return CodeError::Good();
	}

	static CodeError ParseValue(TokenIter& p, Expression& out) {
		Expression value{};
		switch (p->type) {
		case Token::Type::Null:
		case Token::Type::Bool:
		case Token::Type::Int:
		case Token::Type::Float:
		case Token::Type::String:
			value.operation = Operation::Literal;
			value.literal = *p;
			break;
		case Token::Type::Word:
			value.operation = Operation::Variable;
			value.literal = *p;
		default:
			return CodeError::Bad("Unexpected expression token", p->srcPos);
		}
		++p;

		Expression postfix{};
		bool parsed = true;
		while (parsed && !p.EndReached()) {
			if (auto error = ParsePostfix(p, value, postfix, parsed)) {
				return error;
			}
			value = std::move(postfix);
			postfix = {};
		}

		out = std::move(value);
		return CodeError::Good();
	}

	static CodeError ParseBracket(TokenIter& p, Expression& out) {
		++p;

		if (auto error = ParseExpression(p, out)) {
			return error;
		}

		if (p.EndReached()) {
			return CodeError::Bad("Expected a ')'", (--p)->srcPos);
		} else if (p->text != ")") {
			return CodeError::Bad("Expected a ')'", p->srcPos);
		}
		++p;

		return CodeError::Good();
	}

	static CodeError ParseListLiteral(TokenIter& p, Expression& out) {
		++p;

		out.operation = Operation::ListLiteral;
		if (p.EndReached()) {
			return CodeError::Bad("Expected an expression", (--p)->srcPos);
		} else if (auto error = ParseExpressionList(p, "]", out.children)) {
			return error;
		}

		return CodeError::Good();
	}

	static CodeError ParseMapLiteral(TokenIter& p, Expression& out) {
		out.operation = Operation::MapLiteral;

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

	static CodeError ParseExpression(TokenIter& p, Expression& out, size_t minPrecedence) {

		if (p->text == "(") {
			return ParseBracket(p, out);
		} else if (p->text == "[") {
			return ParseListLiteral(p, out);
		} else if (p->text == "{") {
			return ParseMapLiteral(p, out);
		} else {
			// TODO
		}

		return CodeError::Good();
	}

	CodeError ParseExpression(TokenIter& p, Expression& out) {
		if (p.EndReached()) {
			return CodeError::Bad("Expected an expression", (--p)->srcPos);
		} else {
			return ParseExpression(p, out, 0);
		}
	}
}
