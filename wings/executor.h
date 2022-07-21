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
		static WObj* Run(WObj** args, int argc, WObj* kwargs, void* userdata);
		WContext* context{};
		RcPtr<std::vector<Instruction>> instructions;
		std::string module;
		std::string prettyName;
		std::vector<std::string> localVariables;
		std::vector<std::string> parameterNames;
		std::vector<WObj*> defaultParameterValues;
		std::optional<std::string> listArgs;
		std::optional<std::string> kwArgs;
		std::unordered_map<std::string, RcPtr<WObj*>> captures;
		RcPtr<std::vector<std::string>> originalSource;
	};

	struct TryFrame {
		size_t exceptJump;
		size_t finallyJump;
		bool isHandlingException;
	};

	struct Executor {
		WObj* Run(WObj** args, int argc, WObj* kwargs);

		void PushStack(WObj* obj);
		WObj* PopStack();
		WObj* PeekStack();
		void ClearStack();
		size_t PopArgFrame();

		void DoInstruction(const Instruction& instr);

		WObj* GetVariable(const std::string& name);
		void SetVariable(const std::string& name, WObj* value);

		WObj* DirectAssign(const AssignTarget& target, WObj* value);

		DefObject* def;
		WContext* context;
		size_t pc{};
		std::vector<WObj*> stack;
		std::stack<size_t> argFrames;
		std::stack<std::vector<WObj*>> kwargsStack;
		std::unordered_map<std::string, RcPtr<WObj*>> variables;
		std::optional<WObj*> exitValue;

		std::stack<TryFrame> tryFrames;
	};

}
