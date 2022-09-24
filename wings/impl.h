#pragma once
#include "rcptr.h"
#include "wings.h"
#include "attributetable.h"
#include "hash.h"
#include <string>
#include <string_view>
#include <vector>
#include <deque>
#include <unordered_set>
#include <unordered_map>
#include <array>
#include <memory>
#include <optional>
#include <type_traits>
#include <cstdlib> // std::abort

using wuint = std::make_unsigned_t<Wg_int>;

struct Wg_Context;

namespace wings {
	size_t Guid();
	void InitLibrary(Wg_Context* context);
	std::string WObjTypeToString(const Wg_Obj* obj);
	void CallErrorCallback(const char* message);

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
		std::string_view tag;
		std::string_view func;
		OwnedTraceFrame ToOwned() const {
			return { srcPos, lineText.data(), tag.data(), func.data() };
		}
	};

	struct HashException : public std::exception {};

	struct WObjRef {
		WObjRef() : obj(nullptr) {}
		explicit WObjRef(Wg_Obj* obj) : obj(obj) { if (obj) Wg_ProtectObject(obj); }
		explicit WObjRef(WObjRef&& other) noexcept : obj(other.obj) { other.obj = nullptr; }
		WObjRef& operator=(WObjRef&& other) noexcept { obj = other.obj; other.obj = nullptr; return *this; }
		WObjRef(const WObjRef&) = delete;
		WObjRef& operator=(const WObjRef&) = delete;
		~WObjRef() { if (obj) Wg_UnprotectObject(obj); }
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

		// Exception types
		Wg_Obj* baseException;
		Wg_Obj* exception;
		Wg_Obj* syntaxError;
		Wg_Obj* nameError;
		Wg_Obj* typeError;
		Wg_Obj* valueError;
		Wg_Obj* attributeError;
		Wg_Obj* lookupError;
		Wg_Obj* indexError;
		Wg_Obj* keyError;
		Wg_Obj* arithmeticError;
		Wg_Obj* overflowError;
		Wg_Obj* zeroDivisionError;
		Wg_Obj* stopIteration;

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
				object, noneType, _bool, _int, _float, str, tuple, list, dict, set,
				func, slice, defaultIter, defaultReverseIter, dictKeysIter, dictValuesIter, dictItemsIter,
				
				baseException, exception, syntaxError, nameError, typeError, valueError,
				attributeError, lookupError, indexError, keyError, arithmeticError, overflowError,
				zeroDivisionError, stopIteration,

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
		Wg_Obj* (*fptr)(Wg_Obj** args, int argc, Wg_Obj* kwargs, void* userdata);
		void* userdata;
		bool isMethod;
		std::string tag;
		std::string prettyName;
	};

	struct Class {
		std::string name;
		Wg_Obj* (*ctor)(Wg_Obj** args, int argc, Wg_Obj* kwargs, void* userdata);
		void* userdata;
		std::vector<Wg_Obj*> bases;
		wings::AttributeTable instanceAttributes;
	};

	std::string type;
	union {
		void* data;
		bool* _bool;
		Wg_int* _int;
		Wg_float* _float;
		std::string* _str;
		std::vector<Wg_Obj*>* _list;
		wings::WDict* _map;
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
	Wg_Config config{};
	size_t lastObjectCountAfterGC = 0;
	std::deque<std::unique_ptr<Wg_Obj>> mem;
	std::unordered_map<const Wg_Obj*, size_t> protectedObjects;
	std::unordered_map<std::string, wings::RcPtr<Wg_Obj*>> globals;
	Wg_Obj* currentException = nullptr;
	std::vector<Wg_Obj*> reprStack;
	std::vector<wings::TraceFrame> currentTrace;
	std::vector<wings::OwnedTraceFrame> exceptionTrace;
	std::string traceMessage;
	wings::Builtins builtins{};
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
