#pragma once
#include "compile.h"
#include "rcptr.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <optional>

namespace wings {

	struct DefObject {
		~DefObject();
		static WObj* Run(WObj** args, int argc, void* userdata);
		WContext* context{};
		RcPtr<std::vector<Instruction>> instructions;
		std::string module;
		std::string prettyName;
		std::vector<std::string> localVariables;
		std::vector<std::string> parameterNames;
		std::vector<WObj*> defaultParameterValues;
		std::unordered_map<std::string, RcPtr<WObj*>> captures;
		RcPtr<std::vector<std::string>> originalSource;
	};

	struct Executor {
		WObj* Run(WObj** args, int argc);

		void PushStack(WObj* obj);
		WObj* PopStack();
		WObj* PeekStack();

		void DoInstruction(const Instruction& instr);

		WObj* GetVariable(const std::string& name);
		void SetVariable(const std::string& name, WObj* value);

		DefObject* def;
		WContext* context;
		size_t pc{};
		std::vector<WObj*> stack;
		std::unordered_map<std::string, RcPtr<WObj*>> variables;
		std::optional<WObj*> exitValue;
	};

}
