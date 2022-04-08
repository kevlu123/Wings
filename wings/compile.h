#pragma once
#include "parse.h"
#include "wings.h"
#include "rcptr.h"
#include <vector>

namespace wings {

	struct OperationInstructionInfo {
		Operation op;
		Token token; // Holds literal, variable, and/or source location of operation
	};

	struct Instruction;

	struct DefInstructionInfo {
		size_t defaultParameterCount;
		std::vector<Parameter> parameters;
		std::vector<std::string> globalCaptures;
		std::vector<std::string> localCaptures;
		std::vector<std::string> variables;
		RcPtr<std::vector<Instruction>> instructions;
	};

	struct Instruction {
		enum class Type {
			Operation,
			Pop,
			Jump,
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
