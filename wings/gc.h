#pragma once
#include "impl.h"

namespace wings {
	WObj* Alloc(WContext* context);
	void DestroyAllObjects(WContext* context);
}
