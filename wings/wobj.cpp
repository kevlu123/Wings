#include "impl.h"
#include "gc.h"
#include "executor.h"
#include <algorithm>

using namespace wings;

bool operator==(const WObj& lhs, const WObj& rhs) {
    if (lhs.type != rhs.type)
        return false;

    switch (lhs.type) {
    case WObj::Type::Null: return true;
    case WObj::Type::Bool: return lhs.b == rhs.b;
    case WObj::Type::Int: return lhs.i == rhs.i;
    case WObj::Type::Float: return lhs.f == rhs.f;
    case WObj::Type::String: return lhs.s == rhs.s;
    case WObj::Type::List:
    case WObj::Type::Map:
    case WObj::Type::Func:
    case WObj::Type::Userdata: return &lhs == &rhs;
    default: WUNREACHABLE();
    }
}

bool operator!=(const WObj& lhs, const WObj& rhs) {
    return !(lhs == rhs);
}

extern "C" {

    WObj* WObjCreateNull(WContext* context) {
        WASSERT(context);
        return context->nullSingleton;
    }

    WObj* WObjCreateBool(WContext* context, bool value) {
        WASSERT(context);
        WObj* _class = context->builtinClasses._bool;
        WObj* instance = _class->fn.fptr(nullptr, 0, _class->fn.userdata);
        if (instance) {
            instance->attributes = _class->c.Copy();
            instance->b = value;
        }
        return instance;
    }

    WObj* WObjCreateInt(WContext* context, wint value) {
        WASSERT(context);
        WObj* _class = context->builtinClasses._int;
        WObj* instance = _class->fn.fptr(nullptr, 0, _class->fn.userdata);
        if (instance) {
            instance->attributes = _class->c.Copy();
            instance->i = value;
        }
        return instance;
    }

    WObj* WObjCreateFloat(WContext* context, wfloat value) {
        WASSERT(context);
        WObj* _class = context->builtinClasses._float;
        WObj* instance = _class->fn.fptr(nullptr, 0, _class->fn.userdata);
        if (instance) {
            instance->attributes = _class->c.Copy();
            instance->f = value;
        }
        return instance;
    }

    WObj* WObjCreateString(WContext* context, const char* value) {
        WASSERT(context && value);
        WObj* _class = context->builtinClasses.str;
        WObj* instance = _class->fn.fptr(nullptr, 0, _class->fn.userdata);
        if (instance) {
            instance->attributes = _class->c.Copy();
            instance->s = value;
        }
        return instance;
    }

    WObj* WObjCreateList(WContext* context) {
        WASSERT(context);
        WObj* _class = context->builtinClasses.list;
        WObj* instance = _class->fn.fptr(nullptr, 0, _class->fn.userdata);
        if (instance) {
            instance->attributes = _class->c.Copy();
        }
        return instance;
    }

    WObj* WObjCreateMap(WContext* context) {
        WASSERT(context);
        WObj* _class = context->builtinClasses.map;
        WObj* instance = _class->fn.fptr(nullptr, 0, _class->fn.userdata);
        if (instance) {
            instance->attributes = _class->c.Copy();
        }
        return instance;
    }

    WObj* WObjCreateObject(WContext* context) {
        WASSERT(context);
        WObj* _class = context->builtinClasses.object;
        WObj* instance = _class->fn.fptr(nullptr, 0, _class->fn.userdata);
        if (instance) {
            instance->attributes = _class->c.Copy();
        }
        return instance;
    }

    WObj* WObjCreateFunc(WContext* context, const WFunc* value) {
        WASSERT(context && value && value->fptr);
        WObj* _class = context->builtinClasses.func;
        WObj* instance = _class->fn.fptr(nullptr, 0, _class->fn.userdata);
        if (instance) {
            instance->attributes = _class->c.Copy();
            instance->fn = *value;
        }
        return instance;
    }

    WObj* WObjCreateUserdata(WContext* context, void* value) {
        WASSERT(context);
        WObj* _class = context->builtinClasses.userdata;
        WObj* instance = _class->fn.fptr(nullptr, 0, _class->fn.userdata);
        if (instance) {
            instance->attributes = _class->c.Copy();
            instance->u = value;
        }
        return instance;
    }

    WObj* WObjCreateClass(WContext* context, const WClass* value) {
        WASSERT(context && value && value->methodCount >= 0);
        if (value->methodCount) {
            WASSERT(value->methods && value->methodNames);
            for (int i = 0; i < value->methodCount; i++) {
                WASSERT(value->methods[i] && value->methodNames[i] && WObjIsFunc(value->methods[i]));
            }
        }

        WObj* _class = Alloc(context);
        if (_class == nullptr) {
            return nullptr;
        }

        _class->type = WObj::Type::Class;
        for (int i = 0; i < value->methodCount; i++) {
            _class->c.Set(value->methodNames[i], value->methods[i]);
        }
        _class->c.Set("__class__", _class);
        _class->c.SetSuper(context->builtinClasses.object->c);

        WFunc constructor{};
        constructor.userdata = _class;
        constructor.isMethod = true;
        constructor.prettyName = "__init__";
        constructor.fptr = [](WObj** argv, int argc, void* userdata) {
            WObj* _class = (WObj*)userdata;
            WContext* context = _class->context;

            WObj* instance = WObjCreateObject(context);
            if (instance == nullptr)
                return (WObj*)nullptr;

            instance->attributes = _class->c.Copy();

            if (WObj* init = _class->c.Get("__init__")) {
                if (WObjIsFunc(init)) {
                    std::vector<WObj*> newArgv = { instance };
                    newArgv.insert(newArgv.end(), argv, argv + argc);
                    WObj* ret = WOpCall(init, newArgv.data(), argc + 1);
                    if (ret == nullptr) {
                        return (WObj*)nullptr;
                    } else if (!WObjIsNull(ret)) {
                        WErrorSetRuntimeError(context, "Constructor returned a non NoneType type");
                        return (WObj*)nullptr;
                    }
                }
            }

            return instance;
        };
        _class->fn = constructor;


        return _class;
    }

    bool WObjIsNull(const WObj* obj) {
        WASSERT(obj);
        return obj->type == WObj::Type::Null;
    }

    bool WObjIsBool(const WObj* obj) {
        WASSERT(obj);
        return obj->type == WObj::Type::Bool;
    }

    bool WObjIsInt(const WObj* obj) {
        WASSERT(obj);
        return obj->type == WObj::Type::Int;
    }

    bool WObjIsIntOrFloat(const WObj* obj) {
        WASSERT(obj);
        return obj->type == WObj::Type::Int || obj->type == WObj::Type::Float;
    }

    bool WObjIsString(const WObj* obj) {
        WASSERT(obj);
        return obj->type == WObj::Type::String;
    }

    bool WObjIsList(const WObj* obj) {
        WASSERT(obj);
        return obj->type == WObj::Type::List;
    }

    bool WObjIsMap(const WObj* obj) {
        WASSERT(obj);
        return obj->type == WObj::Type::Map;
    }

    bool WObjIsObject(const WObj* obj) {
        WASSERT(obj);
        return obj->type == WObj::Type::Object;
    }

    bool WObjIsClass(const WObj* obj) {
        WASSERT(obj);
        return obj->type == WObj::Type::Class;
    }

    bool WObjIsFunc(const WObj* obj) {
        WASSERT(obj);
        return obj->type == WObj::Type::Func;
    }

    bool WObjIsUserdata(const WObj* obj) {
        WASSERT(obj);
        return obj->type == WObj::Type::Userdata;
    }

    bool WObjIsImmutableType(const WObj* obj) {
        return obj->type == WObj::Type::Null
            || obj->type == WObj::Type::Bool
            || obj->type == WObj::Type::Int
            || obj->type == WObj::Type::Float
            || obj->type == WObj::Type::String;
    }

    bool WObjGetBool(const WObj* obj) {
        WASSERT(obj && WObjIsBool(obj));
        return obj->b;
    }

    wint WObjGetInt(const WObj* obj) {
        WASSERT(obj && WObjIsInt(obj));
        return obj->i;
    }

    wfloat WObjGetFloat(const WObj* obj) {
        WASSERT(obj && WObjIsIntOrFloat(obj));
        switch (obj->type) {
        case WObj::Type::Int: return (wfloat)obj->i;
        case WObj::Type::Float: return obj->f;
        default: WUNREACHABLE();
        }
    }

    const char* WObjGetString(const WObj* obj) {
        WASSERT(obj && WObjIsString(obj));
        return obj->s.c_str();
    }

    void WObjGetFunc(const WObj* obj, WFunc* fn) {
        WASSERT(obj && fn && WObjIsFunc(obj));
        *fn = obj->fn;
    }

    void* WObjGetUserdata(const WObj* obj) {
        WASSERT(obj && WObjIsUserdata(obj));
        return obj->u;
    }

	void WObjGetFinalizer(const WObj* obj, WFinalizer* finalizer) {
		WASSERT(obj && finalizer);
		*finalizer = obj->finalizer;
	}

	void WObjSetFinalizer(WObj* obj, const WFinalizer* finalizer) {
		WASSERT(obj && finalizer);
		obj->finalizer = *finalizer;
	}

    WObj* WObjGetAttribute(WObj* obj, const char* member) {
        WASSERT(obj && member);
        WObj* mem = obj->attributes.Get(member);
        if (mem && WObjIsFunc(mem) && mem->fn.isMethod) {
            mem->self = obj;
        }
        return mem;
    }

    void WObjSetAttribute(WObj* obj, const char* member, WObj* value) {
        WASSERT(obj && member && value);
        obj->attributes.Set(member, value);
    }

    bool WOpIterate(WObj* obj, void* userdata, bool(*callback)(WObj*, void*)) {
        WASSERT(obj && callback);
        WObj* iter = WOpCallMethod(obj, "__iter__", nullptr, 0);
        if (iter == nullptr) {
            WErrorSetRuntimeError(obj->context, "Object is not iterable (does not implement the __iter__ method)");
            return false;
        }
        WGcProtect(iter);

        while (true) {
            WObj* endReached = WOpCallMethod(iter, "__end__", nullptr, 0);
            if (endReached == nullptr) {
                WErrorSetRuntimeError(obj->context, "Iterator does not implement the __end__ method");
                WGcUnprotect(iter);
                return false;
            }

            WObj* truthy = WOpTruthy(endReached);
            if (truthy == nullptr) {
                WGcUnprotect(iter);
                return false;
            } else if (WObjGetBool(truthy)) {
                WGcUnprotect(iter);
                return true;
            }

            WObj* value = WOpCallMethod(iter, "__next__", nullptr, 0);
            if (endReached == nullptr) {
                WErrorSetRuntimeError(obj->context, "Iterator does not implement the __next__ method");
                WGcUnprotect(iter);
                return false;
            }

            if (!callback(value, userdata)) {
                WGcUnprotect(iter);
                return false;
            }
        }
    }

    WObj* WOpTruthy(WObj* arg) {
        if (WObj* res = WOpCallMethod(arg, "__nonzero__", nullptr, 0)) {
            if (WObjIsBool(res)) {
                return res;
            } else {
                WErrorSetRuntimeError(arg->context, "__nonzero__() returned a non bool type");
            }
        }
        return nullptr;
    }

    WObj* WOpCastToInt(WObj* arg) {
        if (WObj* res = WOpCallMethod(arg, "__int__", nullptr, 0)) {
            if (WObjIsInt(res)) {
                return res;
            } else {
                WErrorSetRuntimeError(arg->context, "__int__() returned a non int type");
            }
        }
        return nullptr;
    }

    WObj* WOpCastToFloat(WObj* arg) {
        if (WObj* res = WOpCallMethod(arg, "__float__", nullptr, 0)) {
            if (WObjIsIntOrFloat(res)) {
                return res;
            } else {
                WErrorSetRuntimeError(arg->context, "__float__() returned a non float type");
            }
        }
        return nullptr;
    }

    WObj* WOpCastToString(WObj* arg) {
        if (WObj* res = WOpCallMethod(arg, "__str__", nullptr, 0)) {
            if (WObjIsString(res)) {
                return res;
            } else {
                WErrorSetRuntimeError(arg->context, "__str__() returned a non str type");
            }
        }
        return nullptr;
    }

    WObj* WOpCall(WObj* callable, WObj** argv, int argc) {
        WASSERT(callable && argc >= 0 && (argc == 0 || argv));
        if (WObjIsFunc(callable) || WObjIsClass(callable)) {
            if (argc)
                WASSERT(argv);
            for (int i = 0; i < argc; i++)
                WASSERT(argv[i]);

            WObj* ret;
            WGcProtect(callable);
            if (callable->self) {
                std::vector<WObj*> argsWithSelf = { callable->self };
                argsWithSelf.insert(argsWithSelf.end(), argv, argv + argc);
                ret = callable->fn.fptr(argsWithSelf.data(), argc + 1, callable->fn.userdata);
            } else {
                ret = callable->fn.fptr(argv, argc, callable->fn.userdata);
            }
            WGcUnprotect(callable);

            if (ret == nullptr && callable->fn.fptr != &DefObject::Run) {
                // Native function failed
                callable->context->err.trace.push_back({
                    {},
                    "",
                    "<Native>",
                    callable->fn.prettyName ? callable->fn.prettyName : "",
                    });
            }

            return ret;
        } else {
            return WOpCallMethod(callable, "__call__", argv, argc);
        }
    }

    WObj* WOpCallMethod(WObj* obj, const char* member, WObj** argv, int argc) {
        WASSERT(obj && member);
        if (argc)
            WASSERT(argv);
        for (int i = 0; i < argc; i++)
            WASSERT(argv[i]);

        WObj* method = WObjGetAttribute(obj, member);
        if (method == nullptr) {
            std::string msg = "Object of type " +
                WObjTypeToString(obj->type) +
                " has no attribute " +
                member;
            WErrorSetRuntimeError(obj->context, msg.c_str());
            return nullptr;
        } else {
            return WOpCall(method, argv, argc);
        }
    }

    WObj* WOpGetIndex(WObj* obj, WObj* index) {
        return WOpCallMethod(obj, "__getitem__", &index, 1);
    }

    WObj* WOpSetIndex(WObj* obj, WObj* index, WObj* value) {
        WObj* argv[2] = { index, value };
        return WOpCallMethod(obj, "__setitem__", argv, 2);
    }

    WObj* WOpPositive(WObj* arg) {
        return WOpCallMethod(arg, "__pos__", nullptr, 0);
    }

    WObj* WOpNegative(WObj* arg) {
        return WOpCallMethod(arg, "__neg__", nullptr, 0);
    }

    WObj* WOpAdd(WObj* lhs, WObj* rhs) {
        return WOpCallMethod(lhs, "__add__", &rhs, 1);
    }

    WObj* WOpSubtract(WObj* lhs, WObj* rhs) {
        return WOpCallMethod(lhs, "__sub__", &rhs, 1);
    }

    WObj* WOpMultiply(WObj* lhs, WObj* rhs) {
        return WOpCallMethod(lhs, "__mul__", &rhs, 1);
    }

    WObj* WOpDivide(WObj* lhs, WObj* rhs) {
        return WOpCallMethod(lhs, "__div__", &rhs, 1);
    }

    WObj* WOpFloorDivide(WObj* lhs, WObj* rhs) {
        return WOpCallMethod(lhs, "__floordiv__", &rhs, 1);
    }

    WObj* WOpModulo(WObj* lhs, WObj* rhs) {
        return WOpCallMethod(lhs, "__mod__", &rhs, 1);
    }

    WObj* WOpPower(WObj* lhs, WObj* rhs) {
        return WOpCallMethod(lhs, "__pow__", &rhs, 1);
    }

    WObj* WOpEquals(WObj* lhs, WObj* rhs) {
        if (WObj* res = WOpCallMethod(lhs, "__eq__", &rhs, 1)) {
            if (WObjIsBool(res)) {
                return res;
            } else {
                WErrorSetRuntimeError(lhs->context, "__eq__() returned a non bool type");
            }
        }
        return nullptr;
    }

    WObj* WOpNotEquals(WObj* lhs, WObj* rhs) {
        if (WObj* res = WOpCallMethod(lhs, "__ne__", &rhs, 1)) {
            if (WObjIsBool(res)) {
                return res;
            } else {
                WErrorSetRuntimeError(lhs->context, "__ne__() returned a non bool type");
            }
        }
        return nullptr;
    }

    WObj* WOpLessThan(WObj* lhs, WObj* rhs) {
        if (WObj* res = WOpCallMethod(lhs, "__lt__", &rhs, 1)) {
            if (WObjIsBool(res)) {
                return res;
            } else {
                WErrorSetRuntimeError(lhs->context, "__lt__() returned a non bool type");
            }
        }
        return nullptr;
    }

    WObj* WOpLessThanOrEqual(WObj* lhs, WObj* rhs) {
        if (WObj* res = WOpCallMethod(lhs, "__le__", &rhs, 1)) {
            if (WObjIsBool(res)) {
                return res;
            } else {
                WErrorSetRuntimeError(lhs->context, "__le__() returned a non bool type");
            }
        }
        return nullptr;
    }

    WObj* WOpGreaterThan(WObj* lhs, WObj* rhs) {
        if (WObj* res = WOpCallMethod(lhs, "__gt__", &rhs, 1)) {
            if (WObjIsBool(res)) {
                return res;
            } else {
                WErrorSetRuntimeError(lhs->context, "__gt__() returned a non bool type");
            }
        }
        return nullptr;
    }

    WObj* WOpGreaterThanOrEqual(WObj* lhs, WObj* rhs) {
        if (WObj* res = WOpCallMethod(lhs, "__ge__", &rhs, 1)) {
            if (WObjIsBool(res)) {
                return res;
            } else {
                WErrorSetRuntimeError(lhs->context, "__ge__() returned a non bool type");
            }
        }
        return nullptr;
    }

    WObj* WOpIn(WObj* container, WObj* obj) {
        if (WObj* res = WOpCallMethod(container, "__contains__", &obj, 1)) {
            if (WObjIsBool(res)) {
                return res;
            } else {
                WErrorSetRuntimeError(container->context, "__contains__() returned a non bool type");
            }
        }
        return nullptr;
    }

    WObj* WOpNotIn(WObj* container, WObj* obj) {
        if (WObj* inOp = WOpIn(container, obj)) {
            return WObjCreateBool(container->context, !WObjGetBool(inOp));
        } else {
            return nullptr;
        }
    }

    WObj* WOpBitAnd(WObj* lhs, WObj* rhs) {
        return WOpCallMethod(lhs, "__and__", &rhs, 1);
    }

    WObj* WOpBitOr(WObj* lhs, WObj* rhs) {
        return WOpCallMethod(lhs, "__or__", &rhs, 1);
    }

    WObj* WOpBitNot(WObj* arg) {
        return WOpCallMethod(arg, "__invert__", nullptr, 0);
    }

    WObj* WOpBitXor(WObj* lhs, WObj* rhs) {
        return WOpCallMethod(lhs, "__xor__", &rhs, 1);
    }

    WObj* WOpShiftLeft(WObj* lhs, WObj* rhs) {
        return WOpCallMethod(lhs, "__lshift__", &rhs, 1);
    }

    WObj* WOpShiftRight(WObj* lhs, WObj* rhs) {
        return WOpCallMethod(lhs, "__rshift__", &rhs, 1);
    }

} // extern "C"

namespace std {
    size_t hash<WObj>::operator()(const WObj& obj) const {
        auto doHash = []<typename T>(const T & val) { return std::hash<T>()(val); };

        switch (obj.type) {
        case WObj::Type::Null:     return doHash(nullptr);
        case WObj::Type::Bool:     return doHash(obj.b);
        case WObj::Type::Int:      return doHash(obj.i);
        case WObj::Type::Float:    return doHash(obj.f);
        case WObj::Type::String:   return doHash(obj.s);
        default: WUNREACHABLE();
        }
    }
}
