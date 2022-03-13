#include "gc.h"

namespace wings {

	extern "C" {

        void WGCProtect(WContext* context, const WObj* obj) {
            WASSERT(context && obj);
            context->protectedObjects.insert(obj);
        }

        void WGCUnprotect(WContext* context, const WObj* obj) {
            WASSERT(context && obj);
            auto it = context->protectedObjects.find(obj);
            WASSERT(it != context->protectedObjects.end());
            context->protectedObjects.erase(it);
        }

	} // extern "C"

} // namespace wings
