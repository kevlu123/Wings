#include "gc.h"
#include <deque>
#include <unordered_set>

using namespace wings;

namespace wings {

    WObj* Alloc(WContext* context) {
        // Check if GC should run
        if (context->mem.size() >= (size_t)(context->config.gcRunFactor * context->lastObjectCountAfterGC)) {
            WGcCollect(context);
        }

        // Allocate new object
        auto obj = std::make_unique<WObj>();
        obj->context = context;

        auto p = obj.get();
        context->mem.push_back(std::move(obj));
        return p;
    }
}

extern "C" {

    void WGcCollect(WContext* context) {
        WASSERT(context);

        std::deque<const WObj*> inUse(context->protectedObjects.begin(), context->protectedObjects.end());
        for (auto& var : context->globals) {
            inUse.push_back(*var.second);
        }

        auto attributeTables = {
            context->attributeTables.object,
            context->attributeTables.null,
            context->attributeTables._bool,
            context->attributeTables._int,
            context->attributeTables._float,
            context->attributeTables.str,
            context->attributeTables.list,
            context->attributeTables.map,
            context->attributeTables.func,
            context->attributeTables.userdata,
        };
        for (auto& table : attributeTables) {
            table.ForEach([&](auto& entry) {
                inUse.push_back(entry.second);
                });
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
                    for (const auto& [_, value] : obj->m) {
                        inUse.push_back(value);
                    }
                    break;
                case WObj::Type::Func:
                    if (obj->self)
                        inUse.push_back(obj->self);
                    break;
                }
                
                obj->attributes.ForEach([&](auto& entry) {
                    inUse.push_back(entry.second);
                    });
            }
        }

        // Call finalizers
        for (auto& obj : context->mem)
            if (obj->finalizer.fptr && !traversed.contains(obj.get()))
                obj->finalizer.fptr(obj.get(), obj->finalizer.userdata);

        // Remove unused objects
        context->mem.erase(
            std::remove_if(
                context->mem.begin(),
                context->mem.end(),
                [&traversed](const auto& obj) { return !traversed.contains(obj.get()); }
            ),
            context->mem.end()
        );

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

    void WGcCreateReference(WObj* parent, WObj* child) {
        WASSERT(parent && child);
        parent->references.push_back(child);
    }
    void WGcRemoveReference(WObj* parent, WObj* child) {
        WASSERT(parent && child);
        auto it = std::find(
            parent->references.begin(),
            parent->references.end(),
            child
        );
        WASSERT(it != parent->references.end());
        parent->references.erase(it);
    }

} // extern "C"

