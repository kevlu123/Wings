#include "timemodule.h"
#include "common.h"

#include <chrono>
#include <thread>

namespace wings {
	namespace timemodule {
		static Wg_Obj* sleep(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			Wg_float secs = Wg_GetFloat(argv[0]);
			int ms = (int)(secs * 1000);
			std::this_thread::sleep_for(std::chrono::milliseconds(ms));
			return Wg_None(context);
		}

		static Wg_Obj* time(Wg_Context* context, Wg_Obj**, int argc) {
			WG_EXPECT_ARG_COUNT(0);
			auto now = std::chrono::system_clock::now();
			auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
			auto value = now_ms.time_since_epoch();
			return Wg_NewFloat(context, (Wg_float)value.count() / 1000);
		}
	}
	
	bool ImportTime(Wg_Context* context) {
		using namespace timemodule;
		try {
			RegisterFunction(context, "time", time);
			RegisterFunction(context, "sleep", sleep);
			
			return true;
		} catch (LibraryInitException&) {
			return false;
		}
	}
}
