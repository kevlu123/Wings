#pragma once
#include "compile.h"
#include "rcptr.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <optional>
#include <stack>

namespace wings {

	struct DefObject {
		~DefObject();
		static Wg_Obj* Run(Wg_Context* context, Wg_Obj** args, int argc);
		Wg_Context* context{};
		RcPtr<std::vector<Instruction>> instructions;
		std::string module;
		std::string prettyName;
		std::vector<std::string> localVariables;
		std::vector<std::string> parameterNames;
		std::vector<Wg_Obj*> defaultParameterValues;
		std::optional<std::string> listArgs;
		std::optional<std::string> kwArgs;
		std::unordered_map<std::string, RcPtr<Wg_Obj*>> captures;
		RcPtr<std::vector<std::string>> originalSource;
	};

	struct TryFrame {
		size_t exceptJump;
		size_t finallyJump;
		bool isHandlingException;
	};

	struct Executor {
		Wg_Obj* Run();

		void PushStack(Wg_Obj* obj);
		Wg_Obj* PopStack();
		Wg_Obj* PeekStack();
		void ClearStack();
		size_t PopArgFrame();

		void DoInstruction(const Instruction& instr);

		Wg_Obj* GetVariable(const std::string& name);
		void SetVariable(const std::string& name, Wg_Obj* value);

		Wg_Obj* DirectAssign(const AssignTarget& target, Wg_Obj* value);

		DefObject* def;
		Wg_Context* context;
		size_t pc{};
		std::vector<Wg_Obj*> stack;
		std::stack<size_t> argFrames;
		std::stack<std::vector<Wg_Obj*>> kwargsStack;
		std::unordered_map<std::string, RcPtr<Wg_Obj*>> variables;
		std::optional<Wg_Obj*> exitValue;

		std::stack<TryFrame> tryFrames;
	};

}
