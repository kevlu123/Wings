#include "sysmodule.h"
#include "common.h"

namespace wings {
	static Wg_Obj* exit(Wg_Context* context, Wg_Obj** argv, int argc) {
		Wg_RaiseException(context, WG_EXC_SYSTEMEXIT);
		return nullptr;
	}
	
	bool ImportSys(Wg_Context* context) {
		try {
			RegisterFunction(context, "exit", exit);
			Wg_SetGlobal(context, "argv", context->argv);
			return true;
		} catch (LibraryInitException&) {
			return false;
		}
	}
}
