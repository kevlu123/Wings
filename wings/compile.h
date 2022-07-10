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
		bool isMethod = false;
		std::vector<Parameter> parameters;
		std::vector<std::string> globalCaptures;
		std::vector<std::string> localCaptures;
		std::vector<std::string> variables;
		RcPtr<std::vector<Instruction>> instructions;
	};

	struct ClassInstruction {
		std::vector<std::string> methodNames;
		size_t baseClassCount;
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
		AssignTarget assignTarget;
	};

	struct TryFrameInstruction {
		size_t exceptJump;
		size_t finallyJump;
	};

	struct Instruction {
		enum class Type {
			Literal,
			Tuple,
			List,
			Map,
			SliceClass,
			Def,
			Class,
			Variable,
			Dot,

			DirectAssign,
			MemberAssign,	// Cannot fail

			Jump,			// Cannot fail
			JumpIfFalse,
			Return,			// Cannot fail

			Raise,
			PushTry,
			PopTry,
			Except,
			CurrentException,
			IsInstance,

			Operation,
			Call,
			Pop,
			And,
			Or,
			Not,
			In,
			NotIn,
			ListComprehension,
		} type{};

		std::unique_ptr<OpInstruction> op;
		std::unique_ptr<VariadicOpInstruction> variadicOp;
		std::unique_ptr<DirectAssignInstruction> directAssign;
		std::unique_ptr<MemberAccessInstruction> memberAccess;
		std::unique_ptr<LiteralInstruction> literal;
		std::unique_ptr<VariableLoadInstruction> variable;
		std::unique_ptr<DefInstruction> def;
		std::unique_ptr<ClassInstruction> _class;
		std::unique_ptr<JumpInstruction> jump;
		std::unique_ptr<TryFrameInstruction> pushTry;

		SourcePosition srcPos;
	};

	std::vector<Instruction> Compile(const Statement& parseTree);
}
