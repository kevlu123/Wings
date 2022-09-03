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

using wuint = std::make_unsigned_t<wint>;

struct WContext;

namespace wings {
	size_t Guid();
	void InitLibrary(WContext* context);
	std::string WObjTypeToString(const WObj* obj);
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
		explicit WObjRef(WObj* obj) : obj(obj) { if (obj) WProtectObject(obj); }
		explicit WObjRef(WObjRef&& other) noexcept : obj(other.obj) { other.obj = nullptr; }
		WObjRef& operator=(WObjRef&& other) noexcept { obj = other.obj; other.obj = nullptr; return *this; }
		WObjRef(const WObjRef&) = delete;
		WObjRef& operator=(const WObjRef&) = delete;
		~WObjRef() { if (obj) WUnprotectObject(obj); }
		WObj* Get() const { return obj; }
	private:
		WObj* obj;
	};

	struct Builtins {
		// Types
		WObj* object;
		WObj* noneType;
		WObj* _bool;
		WObj* _int;
		WObj* _float;
		WObj* str;
		WObj* tuple;
		WObj* list;
		WObj* dict;
		WObj* set;
		WObj* func;
		WObj* slice;
		WObj* defaultIter;
		WObj* defaultReverseIter;

		// Exception types
		WObj* baseException;
		WObj* exception;
		WObj* syntaxError;
		WObj* nameError;
		WObj* typeError;
		WObj* valueError;
		WObj* attributeError;
		WObj* lookupError;
		WObj* indexError;
		WObj* keyError;
		WObj* arithmeticError;
		WObj* overflowError;
		WObj* zeroDivisionError;
		WObj* stopIteration;

		// Functions
		WObj* isinstance;
		WObj* repr;
		WObj* hash;
		WObj* len;

		// Instances
		WObj* none;
		WObj* _true;
		WObj* _false;
		WObj* memoryErrorInstance;

		auto GetAll() const {
			return std::array{
				object, noneType, _bool, _int, _float, str, tuple, list, dict, set,
				func, slice, defaultIter, defaultReverseIter,
				
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

struct WObj {
	struct Func {
		WObj* self;
		WObj* (*fptr)(WObj** args, int argc, WObj* kwargs, void* userdata);
		void* userdata;
		bool isMethod;
		std::string tag;
		std::string prettyName;
	};

	struct Class {
		std::string name;
		WObj* (*ctor)(WObj** args, int argc, WObj* kwargs, void* userdata);
		void* userdata;
		std::vector<WObj*> bases;
		wings::AttributeTable instanceAttributes;
	};

	std::string type;
	union {
		void* data;
		bool* _bool;
		wint* _int;
		wfloat* _float;
		std::string* _str;
		std::vector<WObj*>* _list;
		wings::WDict* _map;
		Func* _func;
		Class* _class;
	};
	template <class T> const T& Get() const { return *(const T*)data; }
	template <class T> T& Get() { return *(T*)data; }

	wings::AttributeTable attributes;
	WFinalizerDesc finalizer{};
	std::vector<WObj*> references;
	WContext* context;
};

struct WContext {
	WConfig config{};
	size_t lastObjectCountAfterGC = 0;
	std::deque<std::unique_ptr<WObj>> mem;
	std::unordered_map<const WObj*, size_t> protectedObjects;
	std::unordered_map<std::string, wings::RcPtr<WObj*>> globals;
	WObj* currentException = nullptr;
	std::vector<WObj*> reprStack;
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
