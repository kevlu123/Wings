#pragma once
#include "compile.h"

namespace wings {

	struct Executor {

		std::vector<Instruction> instructions;
		WContext* context{};

		static WObj* Run(WObj** args, int argc, void* userdata);
		WObj* Run(WObj** args, int argc);
	};

}
