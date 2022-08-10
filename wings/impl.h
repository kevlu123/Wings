#pragma once
#include "rcptr.h"
#include "wings.h"
#include "attributetable.h"
#include "wdict.h"
#include <string>
#include <vector>
#include <deque>
#include <unordered_set>
#include <unordered_map>
#include <array>
#include <memory>
#include <optional>
#include <cstdlib> // std::abort

struct WContext;

namespace wings {
    size_t Guid();
    bool InitLibrary(WContext* context);
    std::string WObjTypeToString(const WObj* obj);

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

    struct TraceFrame {
        SourcePosition srcPos;
        std::string lineText;
        std::string module;
        std::string func;
    };

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
        WObj* func;
        WObj* slice;

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

        // Instances
        WObj* none;
        WObj* memoryErrorInstance;
        WObj* isinstance;

        auto GetAll() const {
            return std::array{
                object, noneType, _bool, _int, _float, str, tuple, list, dict, func, slice,
                baseException, exception, syntaxError, nameError, typeError, valueError,
                attributeError, lookupError, indexError, keyError, arithmeticError, overflowError,
                zeroDivisionError,
                none, memoryErrorInstance, isinstance,
            };
        }
    };
}

struct WObj {
    struct Func {
        WObj* self;
        WObj* (*fptr)(WObj** args, int argc, WObj* kwargs, void* userdata);
        void* userdata;
        bool isMethod;
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
    WFinalizer finalizer{};
    std::vector<WObj*> references;
    WContext* context;
};

struct WContext {
    WConfig config{};
    size_t lastObjectCountAfterGC = 0;
    std::deque<std::unique_ptr<WObj>> mem;
    std::unordered_multiset<const WObj*> protectedObjects;
    std::unordered_map<std::string, wings::RcPtr<WObj*>> globals;
    WObj* currentException = nullptr;
    std::vector<WObj*> reprStack;
    std::vector<wings::TraceFrame> trace;
    std::string traceMessage;
    wings::Builtins builtins{};
};

#define WASSERT(assertion) do { if (!(assertion)) std::abort(); } while (0)
#define WUNREACHABLE() std::abort()
