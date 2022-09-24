#pragma once
#include "impl.h"

namespace wings {
	Wg_Obj* Alloc(Wg_Context* context);
	void DestroyAllObjects(Wg_Context* context);
}
