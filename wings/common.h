#pragma once
#include "wings.h"
#include "rcptr.h"
#include "hash.h"
#include "attributetable.h"

#include <string>
#include <string_view>
#include <vector>
#include <deque>
#include <stack>
#include <unordered_map>
#include <array>
#include <memory>
#include <type_traits>
#include <cstdlib>
#include <mutex>

namespace wings {
	extern Wg_ErrorCallback errorCallback;
	extern void* errorCallbackUserdata;
	extern std::mutex errorCallbackMutex;
	
	size_t Guid();
	std::string WObjTypeToString(const Wg_Obj* obj);
	void CallErrorCallback(const char* message);
	Wg_Obj* Alloc(Wg_Context* context);
	void DestroyAllObjects(Wg_Context* context);
	
	struct WObjHasher {
		size_t operator()(Wg_Obj* obj) const;
	};

	struct WObjComparer {
		bool operator()(Wg_Obj* lhs, Wg_Obj* rhs) const;
	};

	using WDict = RelaxedMap<Wg_Obj*, Wg_Obj*, WObjHasher, WObjComparer>;
	using WSet = RelaxedSet<Wg_Obj*, WObjHasher, WObjComparer>;

	struct SourcePosition {
		size_t line = (size_t)-1;
		size_t column = (size_t)-1;
	};

	struct CodeError {
		bool good = true;
		SourcePosition srcPos{};
		std::string message;

		operator bool() const;
		std::string ToString() const;
		static CodeError Good();
		static CodeError Bad(std::string message, SourcePosition srcPos = {});
	};

	struct OwnedTraceFrame {
		SourcePosition srcPos;
		std::string lineText;
		std::string tag;
		std::string func;
	};

	struct TraceFrame {
		SourcePosition srcPos;
		std::string_view lineText;
		std::string_view module;
		std::string_view func;
		OwnedTraceFrame ToOwned() const {
			return { srcPos, lineText.data(), module.data(), func.data() };
		}
	};

	struct HashException : public std::exception {};

	struct Wg_ObjRef {
		Wg_ObjRef() : obj(nullptr) {}
		explicit Wg_ObjRef(Wg_Obj* obj) : obj(obj) { if (obj) Wg_ProtectObject(obj); }
		explicit Wg_ObjRef(Wg_ObjRef&& other) noexcept : obj(other.obj) { other.obj = nullptr; }
		Wg_ObjRef& operator=(Wg_ObjRef&& other) noexcept { obj = other.obj; other.obj = nullptr; return *this; }
		Wg_ObjRef(const Wg_ObjRef&) = delete;
		Wg_ObjRef& operator=(const Wg_ObjRef&) = delete;
		~Wg_ObjRef() { if (obj) Wg_UnprotectObject(obj); }
		Wg_Obj* Get() const { return obj; }
	private:
		Wg_Obj* obj;
	};

	struct Builtins {
		// Types
		Wg_Obj* object;
		Wg_Obj* noneType;
		Wg_Obj* _bool;
		Wg_Obj* _int;
		Wg_Obj* _float;
		Wg_Obj* str;
		Wg_Obj* tuple;
		Wg_Obj* list;
		Wg_Obj* dict;
		Wg_Obj* set;
		Wg_Obj* func;
		Wg_Obj* slice;
		Wg_Obj* defaultIter;
		Wg_Obj* defaultReverseIter;
		Wg_Obj* dictKeysIter;
		Wg_Obj* dictValuesIter;
		Wg_Obj* dictItemsIter;
		Wg_Obj* setIter;
		Wg_Obj* codeObject;
		Wg_Obj* moduleObject;

		// Exception types
		Wg_Obj* baseException;
		Wg_Obj* systemExit;
		Wg_Obj* exception;
		Wg_Obj* stopIteration;
		Wg_Obj* arithmeticError;
		Wg_Obj* overflowError;
		Wg_Obj* zeroDivisionError;
		Wg_Obj* attributeError;
		Wg_Obj* importError;
		Wg_Obj* syntaxError;
		Wg_Obj* lookupError;
		Wg_Obj* indexError;
		Wg_Obj* keyError;
		Wg_Obj* memoryError;
		Wg_Obj* nameError;
		Wg_Obj* runtimeError;
		Wg_Obj* notImplementedError;
		Wg_Obj* recursionError;
		Wg_Obj* typeError;
		Wg_Obj* valueError;

		// Functions
		Wg_Obj* isinstance;
		Wg_Obj* repr;
		Wg_Obj* hash;
		Wg_Obj* len;

		// Instances
		Wg_Obj* none;
		Wg_Obj* _true;
		Wg_Obj* _false;
		Wg_Obj* memoryErrorInstance;

		auto GetAll() const {
			return std::array{
				object, noneType, _bool, _int, _float, str, tuple, list,
				dict, set, func, slice, defaultIter, defaultReverseIter,
				dictKeysIter, dictValuesIter, dictItemsIter, setIter,
				codeObject, moduleObject,

				baseException, systemExit, exception, stopIteration, arithmeticError,
				overflowError, zeroDivisionError, attributeError, importError,
				syntaxError, lookupError, indexError, keyError, memoryError,
				nameError, runtimeError, notImplementedError, recursionError,
				typeError, valueError,

				isinstance, repr,

				none, _true, _false, memoryErrorInstance,
			};
		}
	};

	constexpr const char* DEFAULT_TAG_NAME = "<unnamed>";
	constexpr const char* DEFAULT_FUNC_NAME = "<unnamed>";
}

struct Wg_Obj {
	struct Func {
		Wg_Obj* self;
		Wg_Function fptr;
		void* userdata;
		bool isMethod;
		std::string module;
		std::string prettyName;
	};

	struct Class {
		std::string name;
		std::string module;
		Wg_Function ctor;
		void* userdata;
		std::vector<Wg_Obj*> bases;
		wings::AttributeTable instanceAttributes;
	};

	std::string type;
	union {
		void* data;

		// For debugging only
		bool* _bool;
		Wg_int* _int;
		Wg_float* _float;
		std::string* _str;
		std::vector<Wg_Obj*>* _list;
		wings::WDict* _map;
		wings::WSet* _set;
		Func* _func;
		Class* _class;
	};
	template <class T> const T& Get() const { return *(const T*)data; }
	template <class T> T& Get() { return *(T*)data; }

	wings::AttributeTable attributes;
	Wg_FinalizerDesc finalizer{};
	std::vector<Wg_Obj*> references;
	Wg_Context* context;
};

struct Wg_Context {
	using Globals = std::unordered_map<std::string, wings::RcPtr<Wg_Obj*>>;
	Wg_Config config{};
	size_t lastObjectCountAfterGC = 0;
	std::deque<std::unique_ptr<Wg_Obj>> mem;
	std::unordered_map<const Wg_Obj*, size_t> protectedObjects;
	std::unordered_map<std::string, Globals> globals;
	Wg_Obj* currentException = nullptr;
	std::vector<Wg_Obj*> reprStack;
	std::vector<wings::TraceFrame> currentTrace;
	std::vector<wings::OwnedTraceFrame> exceptionTrace;
	std::string traceMessage;
	wings::Builtins builtins{};
	std::vector<Wg_Obj*> kwargs;
	std::vector<void*> userdata;
	std::unordered_map<std::string, Wg_ModuleLoader> moduleLoaders;
	std::stack<std::string_view> currentModule;
	std::string importPath;
};

#define STRINGIZE_HELPER(x) STRINGIZE2_HELPER(x)
#define STRINGIZE2_HELPER(x) #x
#define LINE_AS_STRING STRINGIZE_HELPER(__LINE__)

#define WUNREACHABLE() std::abort()
#define WASSERT_RET(ret, assertion) do { if (!(assertion)) { wings::CallErrorCallback( \
LINE_AS_STRING " " __FILE__ " " #assertion \
); return ret; } } while (0)
#define WASSERT(assertion) WASSERT_RET({}, assertion)
#define WASSERT_VOID(assertion) WASSERT_RET(void(), assertion)
