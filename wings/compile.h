#pragma once
#include "parse.h"
#include "wings.h"
#include <vector>

namespace wings {

	struct OperationInstructionInfo {
		Operation op;
		Token token; // Holds literal, variable, and/or source location of operation
	};

	struct DefInstructionInfo {
		std::vector<Parameter> parameters;
		std::vector<std::string> globalCaptures;
		std::vector<std::string> localCaptures;
		std::vector<std::string> variables;
	};

	struct Instruction {
		enum class Type {
			Operation,
			Push,
			Pop,
			Jump,
			JumpIfTrue,
			JumpIfFalse,
			Def,
			Return,
		} type{};

		union {
			OperationInstructionInfo* operation = nullptr;
			DefInstructionInfo* def;
			struct {
				// Location to execute next
				size_t location;
			} jump;
			struct {
				// Distance from the next unused index on the stack
				size_t offset;
			} push;
		} data;

		Instruction() = default;
		Instruction(const Instruction&) = delete;
		Instruction(Instruction&&) noexcept;
		Instruction& operator=(const Instruction&) = delete;
		Instruction& operator=(Instruction&&) noexcept;
		~Instruction();
	};

	std::vector<Instruction> Compile(const Statement& parseTree);
}
