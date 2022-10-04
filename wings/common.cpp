#include "common.h"

#include <algorithm>
#include <unordered_set>

namespace wings {

	Wg_ErrorCallback errorCallback;
	void* errorCallbackUserdata;
	std::mutex errorCallbackMutex;
	
	Wg_Obj* Alloc(Wg_Context* context) {
		// Check allocation limits
		if (context->mem.size() >= context->config.maxAlloc) {
			// Too many objects. Try to free up objects
			Wg_CollectGarbage(context);
			if (context->mem.size() >= context->config.maxAlloc) {
				// If there are still too many objects then set a MemoryException
				Wg_RaiseExceptionObject(context->builtins.memoryErrorInstance);
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

	void CallErrorCallback(const char* message) {
		errorCallbackMutex.lock();
		auto cb = errorCallback;
		auto ud = errorCallbackUserdata;
		errorCallbackMutex.unlock();

		if (cb) {
			cb(message, ud);
		} else {
			std::abort();
		}
	}

	size_t Guid() {
		static std::atomic_size_t i = 0;
		return ++i;
	}

	std::string WObjTypeToString(const Wg_Obj* obj) {
		if (Wg_IsNone(obj)) {
			return "NoneType";
		} else if (Wg_IsBool(obj)) {
			return "bool";
		} else if (Wg_IsInt(obj)) {
			return "int";
		} else if (Wg_IsIntOrFloat(obj)) {
			return "float";
		} else if (Wg_IsString(obj)) {
			return "str";
		} else if (Wg_IsTuple(obj)) {
			return "tuple";
		} else if (Wg_IsList(obj)) {
			return "list";
		} else if (Wg_IsDictionary(obj)) {
			return "dict";
		} else if (Wg_IsSet(obj)) {
			return "set";
		} else if (Wg_IsFunction(obj)) {
			return "function";
		} else if (Wg_IsClass(obj)) {
			return "class";
		} else if (obj->type == "__object") {
			return "object";
		} else {
			return obj->type;
		}
	}

	std::string CodeError::ToString() const {
		if (good) {
			return "Success";
		} else {
			return '(' + std::to_string(srcPos.line + 1) + ','
				+ std::to_string(srcPos.column + 1) + ") "
				+ message;
		}
	}

	CodeError::operator bool() const {
		return !good;
	}

	CodeError CodeError::Good() {
		return CodeError{ true };
	}

	CodeError CodeError::Bad(std::string message, SourcePosition srcPos) {
		return CodeError{
			.good = false,
			.srcPos = srcPos,
			.message = message
		};
	}

	size_t WObjHasher::operator()(Wg_Obj* obj) const {
		if (Wg_Obj* hash = Wg_UnaryOp(WG_UOP_HASH, obj))
			return (size_t)Wg_GetInt(hash);
		throw HashException();
	}

	bool WObjComparer::operator()(Wg_Obj* lhs, Wg_Obj* rhs) const {
		if (Wg_Obj* eq = Wg_BinaryOp(WG_BOP_EQ, lhs, rhs))
			return Wg_GetBool(eq);
		throw HashException();
	}

	static const std::unordered_set<std::string_view> RESERVED = {
		"True", "False", "None",
		"and", "or", "not",
		"if", "else", "elif", "while", "for",
		"class", "def",
		"try", "except", "finally", "raise", "with", "assert",
		"return", "break", "continue", "pass",
		"global", "nonlocal", "del",
		"from", "import",
		"lambda", "in", "as", "is",
		"await", "async", "yield",
	};

	bool IsKeyword(std::string_view s) {
		return RESERVED.contains(s);
	}

	bool IsValidIdentifier(std::string_view s) {
		if (s.empty())
			return false;

		auto isalpha = [](char c) {
			return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
		};

		auto isalnum = [isalpha](char c) {
			return isalpha(c) || (c >= '0' && c <= '9');
		};
		
		return isalpha(s[0])
			&& std::all_of(s.begin() + 1, s.end(), isalnum)
			&& !IsKeyword(s);
	}
}
