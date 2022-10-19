#pragma once
#include "wings.h"
#include "parse.h"
#include "rcptr.h"

#include <string>
#include <vector>
#include <variant>
#include <memory>

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

	using LiteralInstruction = std::variant<std::nullptr_t, bool, Wg_int, Wg_float, std::string>;

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

	struct ImportInstruction {
		std::string module;
		std::string alias;
	};

	struct ImportFromInstruction {
		std::string module;
		std::vector<std::string> names;
		std::string alias;
	};

	struct Instruction {
		enum class Type {
			Literal,
			Tuple, List, Map, Set,
			Slice,
			Def,
			Class,
			Variable,
			Dot,
			Import,
			ImportFrom,

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
			And, Or, Not,
			Is,
		} type{};

		std::unique_ptr<DirectAssignInstruction> directAssign;
		std::unique_ptr<LiteralInstruction> literal;
		std::unique_ptr<StringArgInstruction> string;
		std::unique_ptr<DefInstruction> def;
		std::unique_ptr<ClassInstruction> klass;
		std::unique_ptr<JumpInstruction> jump;
		std::unique_ptr<TryFrameInstruction> pushTry;
		std::unique_ptr<ImportInstruction> import;
		std::unique_ptr<ImportFromInstruction> importFrom;

		SourcePosition srcPos;
	};

	std::vector<Instruction> Compile(const Statement& parseTree);
}
