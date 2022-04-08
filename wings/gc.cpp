#include "gc.h"
#include <deque>
#include <unordered_set>

namespace wings {

    WObj* Alloc(WContext* context) {
        // Check if GC should run
        if (context->mem.size() >= (size_t)(context->config.gcRunFactor * context->lastObjectCountAfterGC)) {
            WGcCollect(context);
        }

        // Allocate new object
        auto obj = std::make_unique<WObj>();
        auto p = obj.get();
        context->mem.push_back(std::move(obj));
        return p;
    }

    extern "C" {

        void WGcCollect(WContext* context) {
            WASSERT(context);

            std::deque<const WObj*> inUse(context->protectedObjects.begin(), context->protectedObjects.end());
            for (auto& var : context->globals) {
                inUse.push_back(*var.second);
            }

            // Recursively find objects in use
            std::unordered_set<const WObj*> traversed;
            while (inUse.size()) {
                auto obj = inUse.back();
                inUse.pop_back();
                if (!traversed.contains(obj)) {
                    traversed.insert(obj);
                    switch (obj->type) {
                    case WObj::Type::List:
                        inUse.insert(
                            inUse.end(),
                            obj->v.begin(),
                            obj->v.end()
                        );
                        break;
                    case WObj::Type::Map:
                        for (const auto& [_, value] : obj->m)
                            inUse.push_back(value);
                        break;
                    case WObj::Type::Func:
                        inUse.insert(
                            inUse.end(),
                            obj->fn.captures,
                            obj->fn.captures + obj->fn.captureCount
                        );
                        break;
                    }
                }
            }

            // Move unused objects to the end of the range
            auto unusedBegin = std::remove_if(
                context->mem.begin(),
                context->mem.end(),
                [&traversed](const auto& obj) { return !traversed.contains(obj.get()); }
            );
            // Call the finalizer for those objects
            for (decltype(unusedBegin) it = unusedBegin; it != context->mem.end(); ++it) {
                const auto& obj = *it;
                if (obj->finalizer.fptr)
                    obj->finalizer.fptr(obj.get(), obj->finalizer.userdata);
            }
            // Free the memory
            context->mem.erase(unusedBegin, context->mem.end());

            context->lastObjectCountAfterGC = context->mem.size();
        }

        void WGcProtect(WContext* context, const WObj* obj) {
            WASSERT(context && obj);
            context->protectedObjects.insert(obj);
        }

        void WGcUnprotect(WContext* context, const WObj* obj) {
            WASSERT(context && obj);
            auto it = context->protectedObjects.find(obj);
            WASSERT(it != context->protectedObjects.end());
            context->protectedObjects.erase(it);
        }

    } // extern "C"

} // namespace wings
