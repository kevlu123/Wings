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
#include <atomic>
#include <random>

static_assert(sizeof(Wg_int) == sizeof(Wg_uint));

namespace wings {
	extern std::atomic<Wg_ErrorCallback> errorCallback;
	
	size_t Guid();
	std::string WObjTypeToString(const Wg_Obj* obj);
	void CallErrorCallback(const char* message);
	Wg_Obj* Alloc(Wg_Context* context);
	bool IsKeyword(std::string_view s);
	bool IsValidIdentifier(std::string_view s);
	Wg_Obj* Compile(Wg_Context* context, const char* code, const char* module, const char* prettyName, bool expr);
	Wg_Obj* Execute(Wg_Context* context, const char* code, const char* module);
	bool InitArgv(Wg_Context* context, const char* const* argv, int argc);
	void RegisterMethod(Wg_Obj* klass, const char* name, Wg_Function fptr);
	Wg_Obj* RegisterFunction(Wg_Context* context, const char* name, Wg_Function fptr);
	void AddAttributeToClass(Wg_Obj* klass, const char* attribute, Wg_Obj* value);

	struct LibraryInitException : std::exception {};

	struct Executor;

	template <class F, class T>
	void RegisterConstant(Wg_Context* context, const char* name, F f, T v) {
		Wg_Obj* obj = f(context, v);
		if (obj == nullptr)
			throw LibraryInitException();
		Wg_SetGlobal(context, name, obj);
	}

	template <class T>
	bool TryGetUserdata(Wg_Obj* obj, const char* type, T** out) {
		return Wg_TryGetUserdata(obj, type, (void**)out);
	}

	struct Rng {
		Rng();
		void Seed(Wg_int seed);
		Wg_float Rand();
		Wg_int Int(Wg_int minIncl, Wg_int maxIncl);
		Wg_float Float(Wg_float minIncl, Wg_float maxIncl);
		std::mt19937_64& Engine();
	private:
		std::mt19937_64 engine;
		std::uniform_real_distribution<Wg_float> dist;
	};
	
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
		std::string module;
		std::string func;
		bool syntaxError;
	};

	struct TraceFrame {
		SourcePosition srcPos;
		std::string_view lineText;
		std::string_view module;
		std::string_view func;
		bool syntaxError = false;
		OwnedTraceFrame ToOwned() const {
			return { srcPos, std::string(lineText), std::string(module), std::string(func), syntaxError };
		}
	};

	struct HashException : public std::exception {};

	struct Wg_ObjRef {
		Wg_ObjRef() : obj(nullptr) {}
		explicit Wg_ObjRef(Wg_Obj* obj) : obj(obj) { if (obj) Wg_IncRef(obj); }
		explicit Wg_ObjRef(Wg_ObjRef&& other) noexcept : obj(other.obj) { other.obj = nullptr; }
		Wg_ObjRef& operator=(Wg_ObjRef&& other) noexcept { obj = other.obj; other.obj = nullptr; return *this; }
		Wg_ObjRef(const Wg_ObjRef&) = delete;
		Wg_ObjRef& operator=(const Wg_ObjRef&) = delete;
		~Wg_ObjRef() { if (obj) Wg_DecRef(obj); }
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
		Wg_Obj* file;
		Wg_Obj* readlineIter;

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
		Wg_Obj* osError;
		Wg_Obj* isADirectoryError;
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
		Wg_Obj* recursionErrorInstance;

		auto GetAll() const {
			return std::array{
				object, noneType, _bool, _int, _float, str, tuple, list,
				dict, set, func, slice, defaultIter, defaultReverseIter,
				dictKeysIter, dictValuesIter, dictItemsIter, setIter,
				codeObject, moduleObject, file, readlineIter,

				baseException, systemExit, exception, stopIteration, arithmeticError,
				overflowError, zeroDivisionError, attributeError, importError,
				syntaxError, lookupError, indexError, keyError, memoryError,
				osError, isADirectoryError, nameError, runtimeError, notImplementedError, recursionError,
				typeError, valueError,

				isinstance, repr, hash, len,

				none, _true, _false, memoryErrorInstance, recursionError,
			};
		}
	};
	
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
		Class* klass;
	};
	template <class T> const T& Get() const { return *(const T*)data; }
	template <class T> T& Get() { return *(T*)data; }

	wings::AttributeTable attributes;
	std::vector<std::pair<Wg_Finalizer, void*>> finalizers;
	Wg_Context* context;
	uint32_t refCount = 0;
};

struct Wg_Context {
	Wg_Config config{};
	wings::Rng rng;
	bool closing = false;
	bool gcRunning = false;
	
	// Garbage collection
	size_t lastObjectCountAfterGC = 0;
	std::vector<std::unique_ptr<Wg_Obj>> mem;
	std::vector<wings::Executor*> executors;

	// Object instances
	using Globals = std::unordered_map<std::string, wings::RcPtr<Wg_Obj*>>;
	std::unordered_map<std::string, Globals> globals;
	wings::Builtins builtins{};
	Wg_Obj* argv = nullptr;
	
	// Exception info
	std::vector<wings::TraceFrame> currentTrace;
	std::vector<wings::OwnedTraceFrame> exceptionTrace;
	std::string traceMessage;
	Wg_Obj* currentException = nullptr;
	
	// Function call data
	std::vector<Wg_Obj*> kwargs;
	std::vector<void*> userdata;
	std::vector<Wg_Obj*> reprStack;

	// Imports
	std::unordered_map<std::string, Wg_ModuleLoader> moduleLoaders;
	std::stack<std::string_view> currentModule;
	std::string importPath;
};

#define WG_UNREACHABLE() std::abort()

#define WG_STRINGIZE_HELPER(x) WG_STRINGIZE2_HELPER(x)
#define WG_STRINGIZE2_HELPER(x) #x
#define WG_LINE_AS_STRING WG_STRINGIZE_HELPER(__LINE__)

// Automatically define WG_NO_ASSERT if compiling in release mode in Visual Studio
#if (defined(_WIN32) && !defined(_DEBUG)) || defined(NDEBUG)
	#ifndef WG_NO_ASSERT
		#define WG_NO_ASSERT
	#endif
#endif

#ifndef WG_NO_ASSERT
	#define WG_ASSERT_RET(ret, assertion) do { if (!(assertion)) { wings::CallErrorCallback( \
	WG_LINE_AS_STRING " " __FILE__ " " #assertion \
	); return ret; } } while (0)
#else
	#define WG_ASSERT_RET(ret, assertion) (void)0
#endif

#define WG_ASSERT(assertion) WG_ASSERT_RET({}, assertion)
#define WG_ASSERT_VOID(assertion) WG_ASSERT_RET(void(), assertion)

#define WG_EXPECT_ARG_COUNT(n) do if (argc != n) { Wg_RaiseArgumentCountError(context, argc, n); return nullptr; } while (0)
#define WG_EXPECT_ARG_COUNT_AT_LEAST(n) do if (argc < n) { Wg_RaiseArgumentCountError(context, argc, n); return nullptr; } while (0)
#define WG_EXPECT_ARG_COUNT_BETWEEN(min, max) do if (argc < min || argc > max) { Wg_RaiseArgumentCountError(context, argc, -1); return nullptr; } while (0)
#define WG_EXPECT_ARG_TYPE(index, check, expect) do if (!(check)(argv[index])) { Wg_RaiseArgumentTypeError(context, index, expect); return nullptr; } while (0)
#define WG_EXPECT_ARG_TYPE_NULL(index) WG_EXPECT_ARG_TYPE(index, Wg_IsNone, "NoneType")
#define WG_EXPECT_ARG_TYPE_BOOL(index) WG_EXPECT_ARG_TYPE(index, Wg_IsBool, "bool")
#define WG_EXPECT_ARG_TYPE_INT(index) WG_EXPECT_ARG_TYPE(index, Wg_IsInt, "int")
#define WG_EXPECT_ARG_TYPE_FLOAT(index) WG_EXPECT_ARG_TYPE(index, [](const Wg_Obj* v) { return Wg_IsIntOrFloat(v) && !Wg_IsInt(v); }, "int or float")
#define WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(index) WG_EXPECT_ARG_TYPE(index, Wg_IsIntOrFloat, "int or float")
#define WG_EXPECT_ARG_TYPE_STRING(index) WG_EXPECT_ARG_TYPE(index, Wg_IsString, "str")
#define WG_EXPECT_ARG_TYPE_LIST(index) WG_EXPECT_ARG_TYPE(index, Wg_IsList, "list")
#define WG_EXPECT_ARG_TYPE_TUPLE(index) WG_EXPECT_ARG_TYPE(index, Wg_IsTuple, "tuple")
#define WG_EXPECT_ARG_TYPE_MAP(index) WG_EXPECT_ARG_TYPE(index, Wg_IsDictionary, "dict")
#define WG_EXPECT_ARG_TYPE_SET(index) WG_EXPECT_ARG_TYPE(index, Wg_IsSet, "set")
#define WG_EXPECT_ARG_TYPE_FUNC(index) WG_EXPECT_ARG_TYPE(index, Wg_IsFunction, "function")
