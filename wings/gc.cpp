#include "gc.h"
#include <deque>
#include <unordered_set>

using namespace wings;

namespace wings {

    WObj* Alloc(WContext* context) {
        // Check if GC should run
        size_t threshold = (size_t)(context->config.gcRunFactor * context->lastObjectCountAfterGC);
        if (context->mem.size() >= threshold) {
            WCollectGarbage(context);
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

    void WCollectGarbage(WContext* context) {
        WASSERT(context);

        if (context->lockGc) {
            return;
        }

        std::deque<const WObj*> inUse(context->protectedObjects.begin(), context->protectedObjects.end());
        for (auto& var : context->globals) {
            inUse.push_back(*var.second);
        }

        auto builtinClasses = {
            context->builtinClasses.object,
            context->builtinClasses.null,
            context->builtinClasses._bool,
            context->builtinClasses._int,
            context->builtinClasses._float,
            context->builtinClasses.str,
            context->builtinClasses.tuple,
            context->builtinClasses.list,
            context->builtinClasses.map,
            context->builtinClasses.func,
            context->builtinClasses.userdata,
            context->builtinClasses.slice,
            context->nullSingleton,
        };
        for (auto& _class : builtinClasses) {
            if (_class) {
                inUse.push_back(_class);
            }
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
                    if (obj->self) {
                        inUse.push_back(obj->self);
                    }
                    break;
                case WObj::Type::Class:
                    obj->c.ForEach([&](auto& entry) {
                        inUse.push_back(entry);
                        });
                    break;
                }
                
                obj->attributes.ForEach([&](auto& entry) {
                    inUse.push_back(entry);
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

    void WProtectObject(const WObj* obj) {
        WASSERT(obj);
        obj->context->protectedObjects.insert(obj);
    }

    void WUnprotectObject(const WObj* obj) {
        WASSERT(obj);
        auto it = obj->context->protectedObjects.find(obj);
        WASSERT(it != obj->context->protectedObjects.end());
        obj->context->protectedObjects.erase(it);
    }

    void WLinkReference(WObj* parent, WObj* child) {
        WASSERT(parent && child);
        parent->references.push_back(child);
    }
    void WUnlinkReference(WObj* parent, WObj* child) {
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

