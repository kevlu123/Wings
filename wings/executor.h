#pragma once
#include "compile.h"
#include <vector>

namespace wings {

	struct Executor {
		static WObj* Run(WObj** args, int argc, void* userdata);
		WObj* Run(WObj** args, int argc);

		void PushStack(WObj* obj);
		WObj* PopStack();

		std::vector<Instruction> instructions;
		WContext* context{};
		std::vector<WObj*> stack;
	};

}
