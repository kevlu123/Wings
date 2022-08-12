#include "gc.h"
#include <deque>
#include <unordered_set>

using namespace wings;

namespace wings {

    WObj* Alloc(WContext* context) {
        // Check allocation limits
        if (context->mem.size() >= context->config.maxAlloc) {
            // Too many objects. Try to free up objects
            WCollectGarbage(context);
            if (context->mem.size() >= context->config.maxAlloc) {
                // If there are still too many objects then set a MemoryException
                WRaiseExceptionObject(context, context->builtins.memoryErrorInstance);
                return nullptr;
            }
        }

        // Check if GC should run
        size_t threshold = (size_t)((double)context->config.gcRunFactor * context->lastObjectCountAfterGC);
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

    void DestroyAllObjects(WContext* context) {
        // Call finalizers
        for (auto& obj : context->mem)
            if (obj->finalizer.fptr)
                obj->finalizer.fptr(obj.get(), obj->finalizer.userdata);

        // Deallocate
        context->mem.clear();
    }
}

extern "C" {

    void WCollectGarbage(WContext* context) {
        WASSERT_VOID(context);

        std::deque<const WObj*> inUse(context->protectedObjects.begin(), context->protectedObjects.end());
        for (auto& var : context->globals) {
            inUse.push_back(*var.second);
        }

        for (auto& obj : context->builtins.GetAll())
            if (obj)
                inUse.push_back(obj);

        // Recursively find objects in use
        std::unordered_set<const WObj*> traversed;
        while (inUse.size()) {
            auto obj = inUse.back();
            inUse.pop_back();
            if (!traversed.contains(obj)) {
                traversed.insert(obj);

                if (WIsTuple(obj) || WIsList(obj)) {
                    inUse.insert(
                        inUse.end(),
                        obj->Get<std::vector<WObj*>>().begin(),
                        obj->Get<std::vector<WObj*>>().end()
                    );
                } else if (WIsDictionary(obj)) {
                    for (const auto& [key, value] : obj->Get<wings::WDict>()) {
                        inUse.push_back(key);
                        inUse.push_back(value);
                    }
                } else if (WIsFunction(obj)) {
                    if (obj->Get<WObj::Func>().self) {
                        inUse.push_back(obj->Get<WObj::Func>().self);
                    }
                } else if (WIsClass(obj)) {
                    inUse.insert(
                        inUse.end(),
                        obj->Get<WObj::Class>().bases.begin(),
                        obj->Get<WObj::Class>().bases.end()
                    );
                    obj->Get<WObj::Class>().instanceAttributes.ForEach([&](auto& entry) {
                        inUse.push_back(entry);
                        });
                }
                
                obj->attributes.ForEach([&](auto& entry) {
                    inUse.push_back(entry);
                    });

                for (WObj* child : obj->references) {
                    inUse.push_back(child);
                }
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
        WASSERT_VOID(obj);
        obj->context->protectedObjects.insert(obj);
    }

    void WUnprotectObject(const WObj* obj) {
        WASSERT_VOID(obj);
        auto it = obj->context->protectedObjects.find(obj);
        WASSERT_VOID(it != obj->context->protectedObjects.end());
        obj->context->protectedObjects.erase(it);
    }

    void WLinkReference(WObj* parent, WObj* child) {
        WASSERT_VOID(parent && child);
        parent->references.push_back(child);
    }
    void WUnlinkReference(WObj* parent, WObj* child) {
        WASSERT_VOID(parent && child);
        auto it = std::find(
            parent->references.begin(),
            parent->references.end(),
            child
        );
        WASSERT_VOID(it != parent->references.end());
        parent->references.erase(it);
    }

} // extern "C"

