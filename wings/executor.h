#pragma once
#include "compile.h"
#include "rcptr.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace wings {

	struct Executor {
		static WObj* Run(WObj** args, int argc, void* userdata);
		WObj* Run(WObj** args, int argc);

		void PushStack(WObj* obj);
		WObj* PopStack();

		bool InitializeParams(WObj** args, int argc);
		void SetLocal(const std::string& name, WObj* value);

		RcPtr<std::vector<Instruction>> instructions;
		WContext* context{};
		std::vector<WObj*> stack;
		std::unordered_map<std::string, RcPtr<WObj*>> variables;
		std::vector<std::string> parameterNames;
		std::vector<WObj*> defaultParameterValues;
	};

}
