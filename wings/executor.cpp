#include "executor.h"

namespace wings {

	WObj* Executor::Run(WObj** args, int argc, void* userdata) {
		return ((Executor*)userdata)->Run(args, argc);
	}

	WObj* Executor::Run(WObj** args, int argc) {
		// TODO
		return nullptr;
	}
}
