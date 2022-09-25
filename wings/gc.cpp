#include "gc.h"
#include <deque>
#include <unordered_set>

namespace wings {

	Wg_Obj* Alloc(Wg_Context* context) {
		// Check allocation limits
		if (context->mem.size() >= context->config.maxAlloc) {
			// Too many objects. Try to free up objects
			Wg_CollectGarbage(context);
			if (context->mem.size() >= context->config.maxAlloc) {
				// If there are still too many objects then set a MemoryException
				Wg_RaiseExceptionObject(context, context->builtins.memoryErrorInstance);
				return nullptr;
			}
		}

		// Check if GC should run
		size_t threshold = (size_t)((double)context->config.gcRunFactor * context->lastObjectCountAfterGC);
		if (context->mem.size() >= threshold) {
			Wg_CollectGarbage(context);
		}

		// Allocate new object
		auto obj = std::make_unique<Wg_Obj>();
		obj->context = context;

		auto p = obj.get();
		context->mem.push_back(std::move(obj));
		return p;
	}

	void DestroyAllObjects(Wg_Context* context) {
		// Call finalizers
		for (auto& obj : context->mem)
			if (obj->finalizer.fptr)
				obj->finalizer.fptr(obj.get(), obj->finalizer.userdata);

		// Deallocate
		context->mem.clear();
	}
}

extern "C" {

	void Wg_CollectGarbage(Wg_Context* context) {
		WASSERT_VOID(context);

		std::deque<const Wg_Obj*> inUse;
		if (context->currentException)
			inUse.push_back(context->currentException);
		for (const auto& [obj, _] : context->protectedObjects)
			inUse.push_back(obj);
		for (auto& var : context->globals)
			inUse.push_back(*var.second);
		for (Wg_Obj* obj : context->kwargs)
			if (obj)
				inUse.push_back(obj);
		for (auto& obj : context->builtins.GetAll())
			if (obj)
				inUse.push_back(obj);

		// Recursively find objects in use
		std::unordered_set<const Wg_Obj*> traversed;
		while (inUse.size()) {
			auto obj = inUse.back();
			inUse.pop_back();
			if (!traversed.contains(obj)) {
				traversed.insert(obj);

				if (Wg_IsTuple(obj) || Wg_IsList(obj)) {
					inUse.insert(
						inUse.end(),
						obj->Get<std::vector<Wg_Obj*>>().begin(),
						obj->Get<std::vector<Wg_Obj*>>().end()
					);
				} else if (Wg_IsDictionary(obj)) {
					for (const auto& [key, value] : obj->Get<wings::WDict>()) {
						inUse.push_back(key);
						inUse.push_back(value);
					}
				} else if (Wg_IsSet(obj)) {
					for (Wg_Obj* value : obj->Get<wings::WSet>()) {
						inUse.push_back(value);
					}
				} else if (Wg_IsFunction(obj)) {
					if (obj->Get<Wg_Obj::Func>().self) {
						inUse.push_back(obj->Get<Wg_Obj::Func>().self);
					}
				} else if (Wg_IsClass(obj)) {
					inUse.insert(
						inUse.end(),
						obj->Get<Wg_Obj::Class>().bases.begin(),
						obj->Get<Wg_Obj::Class>().bases.end()
					);
					obj->Get<Wg_Obj::Class>().instanceAttributes.ForEach([&](auto& entry) {
						inUse.push_back(entry);
						});
				}
				
				obj->attributes.ForEach([&](auto& entry) {
					inUse.push_back(entry);
					});

				for (Wg_Obj* child : obj->references) {
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

	void Wg_ProtectObject(const Wg_Obj* obj) {
		WASSERT_VOID(obj);
		size_t& refCount = obj->context->protectedObjects[obj];
		refCount++;
	}

	void Wg_UnprotectObject(const Wg_Obj* obj) {
		WASSERT_VOID(obj);
		auto it = obj->context->protectedObjects.find(obj);
		WASSERT_VOID(it != obj->context->protectedObjects.end());
		if (it->second == 1) {
			obj->context->protectedObjects.erase(it);
		} else {
			it->second--;
		}
	}

	void Wg_LinkReference(Wg_Obj* parent, Wg_Obj* child) {
		WASSERT_VOID(parent && child);
		parent->references.push_back(child);
	}
	
	void Wg_UnlinkReference(Wg_Obj* parent, Wg_Obj* child) {
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

