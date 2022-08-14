#pragma once
#include "parse.h"
#include "wings.h"
#include "rcptr.h"
#include <vector>
#include <variant>

namespace wings {
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
		std::optional<std::string> listArgs;
		std::optional<std::string> kwArgs;
	};

	struct ClassInstruction {
		std::vector<std::string> methodNames;
		std::string prettyName;
	};

	using LiteralInstruction = std::variant<std::nullptr_t, bool, wint, wfloat, std::string>;

	struct StringArgInstruction {
		std::string string;
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
			MemberAssign,

			Jump,
			JumpIfFalse,
			Return,

			Raise,
			PushTry,
			PopTry,
			Except,
			CurrentException,
			IsInstance,

			Call,
			PushArgFrame,
			Unpack,
			UnpackMapForMapCreation,
			UnpackMapForCall,
			PushKwarg,

			Operation,
			Pop,
			And,
			Or,
			Not,
			In,
			NotIn,
			Is,
			IsNot,
			ListComprehension,
		} type{};

		std::unique_ptr<DirectAssignInstruction> directAssign;
		std::unique_ptr<LiteralInstruction> literal;
		std::unique_ptr<StringArgInstruction> string;
		std::unique_ptr<DefInstruction> def;
		std::unique_ptr<ClassInstruction> _class;
		std::unique_ptr<JumpInstruction> jump;
		std::unique_ptr<TryFrameInstruction> pushTry;

		SourcePosition srcPos;
	};

	std::vector<Instruction> Compile(const Statement& parseTree);
}
