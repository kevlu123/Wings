#pragma once
#include "parse.h"
#include "wings.h"
#include "rcptr.h"
#include <vector>
#include <variant>

namespace wings {

	struct OpInstruction {
		std::string operation;
		size_t argc{};
	};

	struct VariadicOpInstruction {
		size_t argc{};
	};

	struct Instruction;

	struct DefInstruction {
		size_t defaultParameterCount{};
		std::string prettyName;
		std::vector<Parameter> parameters;
		std::vector<std::string> globalCaptures;
		std::vector<std::string> localCaptures;
		std::vector<std::string> variables;
		RcPtr<std::vector<Instruction>> instructions;
	};

	using LiteralInstruction = std::variant<std::nullptr_t, bool, wint, wfloat, std::string>;

	struct VariableLoadInstruction {
		std::string variableName;
	};

	struct MemberAccessInstruction {
		std::string memberName;
	};

	struct JumpInstruction {
		size_t location;
	};

	struct DirectAssignInstruction {
		std::string variableName;
	};

	struct Instruction {
		enum class Type {
			Literal,
			ListLiteral,
			MapLiteral,
			Def,
			Variable,
			Dot,

			DirectAssign,
			MemberAssign,

			Jump,
			JumpIfFalse,
			Return,

			Operation,
			Call,
			Pop,
			And,
			Or,
			Not,
			In,
			NotIn,
		} type{};

		std::unique_ptr<OpInstruction> op;
		std::unique_ptr<VariadicOpInstruction> variadicOp;
		std::unique_ptr<DirectAssignInstruction> directAssign;
		std::unique_ptr<MemberAccessInstruction> memberAccess;
		std::unique_ptr<LiteralInstruction> literal;
		std::unique_ptr<VariableLoadInstruction> variable;
		std::unique_ptr<DefInstruction> def;
		std::unique_ptr<JumpInstruction> jump;

		size_t traceLine;
	};

	std::vector<Instruction> Compile(const Statement& parseTree);
}
