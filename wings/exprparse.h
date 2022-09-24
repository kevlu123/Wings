#pragma once
#include "lex.h"
#include <vector>
#include <optional>
#include <unordered_set>

namespace wings {

	enum class Operation {
		Literal, Variable,
		Tuple, List, Map, Set,
		ListComprehension,
		Index, Call, Slice,
		Pos, Neg,
		Add, Sub, Mul, Div, IDiv, Mod, Pow,
		Eq, Ne, Lt, Le, Gt, Ge,
		And, Or, Not,
		In, NotIn, Is, IsNot,
		BitAnd, BitOr, BitNot, BitXor,
		ShiftL, ShiftR,
		IfElse,
		Assign, AddAssign, SubAssign, MulAssign,
		DivAssign, IDivAssign, ModAssign, PowAssign,
		AndAssign, OrAssign, XorAssign,
		ShiftLAssign, ShiftRAssign,
		Dot,
		Function,
		Unpack, UnpackMapForMapCreation, UnpackMapForCall,
		Kwarg,

		CompoundAssignment,
	};

	enum class AssignType {
		None,
		// var = value
		Direct,
		// var[index] = value
		Index,
		// var.member = value
		Member,
		// (x, y) = (a, b)
		Pack,
	};

	struct AssignTarget {
		AssignType type{}; // Either Direct or Pack
		std::string direct;
		std::vector<AssignTarget> pack;
	};

	struct LiteralValue {
		enum class Type {
			Null,
			Bool,
			Int,
			Float,
			String,
		} type;

		struct {
			union {
				bool b;
				Wg_int i;
				Wg_float f;
			};
			std::string s;
		};
	};

	struct Statement;
	struct Parameter;

	struct Expression {
		Operation operation{};
		std::vector<Expression> children;
		SourcePosition srcPos;

		AssignTarget assignTarget;
		std::string variableName;
		LiteralValue literalValue;
		struct {
			std::string name;
			mutable std::vector<Parameter> parameters;
			std::unordered_set<std::string> globalCaptures;
			std::unordered_set<std::string> localCaptures;
			std::unordered_set<std::string> variables;
			std::vector<Statement> body;
		} def;

		Expression() = default;
		Expression(Expression&&) = default;
		Expression& operator=(Expression&&) = default;
	};

	struct Parameter {
		std::string name;
		std::optional<Expression> defaultValue;
		enum class Type { Named, ListArgs, Kwargs } type = Type::Named;
	};

	struct TokenIter {
		TokenIter(const std::vector<Token>& tokens);
		TokenIter& operator++();
		TokenIter& operator--();
		const Token& operator*() const;
		const Token* operator->() const;
		bool operator==(const TokenIter& rhs) const;
		bool operator!=(const TokenIter& rhs) const;
		bool EndReached() const;
	private:
		size_t index;
		const std::vector<Token>* tokens;
	};

	CodeError ParseExpression(TokenIter& p, Expression& out, bool disableInOp = false);
	CodeError ParseExpressionList(TokenIter& p, const std::string& terminate, std::vector<Expression>& out, bool isFnCall = false, bool* seenComma = nullptr);
	bool IsAssignableExpression(const Expression& expr, AssignTarget& target, bool onlyDirectOrPack = false);

}
