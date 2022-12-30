#include "sysmodule.h"
#include "common.h"

namespace wings {
	namespace sysmodule {
		static Wg_Obj* exit(Wg_Context* context, Wg_Obj**, int) {
			Wg_RaiseException(context, WG_EXC_SYSTEMEXIT);
			return nullptr;
		}
	}
	
	bool ImportSys(Wg_Context* context) {
		using namespace sysmodule;
		try {
			Wg_Obj* list = Wg_NewList(context);
			if (list == nullptr)
				return false;
			Wg_SetGlobal(context, "argv", list);

			if (context->argv.empty()) {
				context->argv.push_back("");
			}

			for (const std::string& arg : context->argv) {
				Wg_Obj* str = Wg_NewString(context, arg.c_str());
				if (str == nullptr)
					return false;
				if (Wg_CallMethod(list, "append", &str, 1) == nullptr)
					return false;
			}

			RegisterFunction(context, "exit", exit);
			return true;
		} catch (LibraryInitException&) {
			return false;
		}
	}
}
