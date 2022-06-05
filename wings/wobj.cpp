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

    WObj* WCreateNoneType(WContext* context) {
        WASSERT(context);
        return context->nullSingleton;
    }

    WObj* WCreateBool(WContext* context, bool value) {
        WASSERT(context);
        WObj* _class = context->builtinClasses._bool;
        WObj* instance = _class->fn.fptr(nullptr, 0, _class->fn.userdata);
        if (instance) {
            instance->attributes = _class->c.Copy();
            instance->b = value;
        }
        return instance;
    }

    WObj* WCreateInt(WContext* context, wint value) {
        WASSERT(context);
        WObj* _class = context->builtinClasses._int;
        WObj* instance = _class->fn.fptr(nullptr, 0, _class->fn.userdata);
        if (instance) {
            instance->attributes = _class->c.Copy();
            instance->i = value;
        }
        return instance;
    }

    WObj* WCreateFloat(WContext* context, wfloat value) {
        WASSERT(context);
        WObj* _class = context->builtinClasses._float;
        WObj* instance = _class->fn.fptr(nullptr, 0, _class->fn.userdata);
        if (instance) {
            instance->attributes = _class->c.Copy();
            instance->f = value;
        }
        return instance;
    }

    WObj* WCreateString(WContext* context, const char* value) {
        WASSERT(context);
        WObj* _class = context->builtinClasses.str;
        WObj* instance = _class->fn.fptr(nullptr, 0, _class->fn.userdata);
        if (instance) {
            instance->attributes = _class->c.Copy();
            instance->s = value ? value : "";
        }
        return instance;
    }

    WObj* WCreateList(WContext* context) {
        WASSERT(context);
        WObj* _class = context->builtinClasses.list;
        WObj* instance = _class->fn.fptr(nullptr, 0, _class->fn.userdata);
        if (instance) {
            instance->attributes = _class->c.Copy();
        }
        return instance;
    }

    WObj* WCreateMap(WContext* context) {
        WASSERT(context);
        WObj* _class = context->builtinClasses.map;
        WObj* instance = _class->fn.fptr(nullptr, 0, _class->fn.userdata);
        if (instance) {
            instance->attributes = _class->c.Copy();
        }
        return instance;
    }

    WObj* WCreateObject(WContext* context) {
        WASSERT(context);
        WObj* _class = context->builtinClasses.object;
        WObj* instance = _class->fn.fptr(nullptr, 0, _class->fn.userdata);
        if (instance) {
            instance->attributes = _class->c.Copy();
        }
        return instance;
    }

    WObj* WCreateFunction(WContext* context, const WFunc* value) {
        WASSERT(context && value && value->fptr);
        WObj* _class = context->builtinClasses.func;
        WObj* instance = _class->fn.fptr(nullptr, 0, _class->fn.userdata);
        if (instance) {
            instance->attributes = _class->c.Copy();
            instance->fn = *value;
        }
        return instance;
    }

    WObj* WCreateUserdata(WContext* context, void* value) {
        WASSERT(context);
        WObj* _class = context->builtinClasses.userdata;
        WObj* instance = _class->fn.fptr(nullptr, 0, _class->fn.userdata);
        if (instance) {
            instance->attributes = _class->c.Copy();
            instance->u = value;
        }
        return instance;
    }

    WObj* WCreateClass(WContext* context, const WClass* value) {
        WASSERT(context && value && value->methodCount >= 0);
        if (value->methodCount) {
            WASSERT(value->methods && value->methodNames);
            for (int i = 0; i < value->methodCount; i++) {
                WASSERT(value->methods[i] && value->methodNames[i] && WIsFunc(value->methods[i]));
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

            WObj* instance = WCreateObject(context);
            if (instance == nullptr)
                return (WObj*)nullptr;

            instance->attributes = _class->c.Copy();

            if (WObj* init = _class->c.Get("__init__")) {
                if (WIsFunc(init)) {
                    std::vector<WObj*> newArgv = { instance };
                    newArgv.insert(newArgv.end(), argv, argv + argc);
                    WObj* ret = WCall(init, newArgv.data(), argc + 1);
                    if (ret == nullptr) {
                        return (WObj*)nullptr;
                    } else if (!WIsNoneType(ret)) {
                        WRaiseError(context, "Constructor returned a non NoneType type");
                        return (WObj*)nullptr;
                    }
                }
            }

            return instance;
        };
        _class->fn = constructor;


        return _class;
    }

    bool WIsNoneType(const WObj* obj) {
        WASSERT(obj);
        return obj->type == WObj::Type::Null;
    }

    bool WIsBool(const WObj* obj) {
        WASSERT(obj);
        return obj->type == WObj::Type::Bool;
    }

    bool WIsInt(const WObj* obj) {
        WASSERT(obj);
        return obj->type == WObj::Type::Int;
    }

    bool WIsIntOrFloat(const WObj* obj) {
        WASSERT(obj);
        return obj->type == WObj::Type::Int || obj->type == WObj::Type::Float;
    }

    bool WIsString(const WObj* obj) {
        WASSERT(obj);
        return obj->type == WObj::Type::String;
    }

    bool WIsList(const WObj* obj) {
        WASSERT(obj);
        return obj->type == WObj::Type::List;
    }

    bool WIsMap(const WObj* obj) {
        WASSERT(obj);
        return obj->type == WObj::Type::Map;
    }

    bool WIsObject(const WObj* obj) {
        WASSERT(obj);
        return obj->type == WObj::Type::Object;
    }

    bool WIsClass(const WObj* obj) {
        WASSERT(obj);
        return obj->type == WObj::Type::Class;
    }

    bool WIsFunc(const WObj* obj) {
        WASSERT(obj);
        return obj->type == WObj::Type::Func;
    }

    bool WIsUserdata(const WObj* obj) {
        WASSERT(obj);
        return obj->type == WObj::Type::Userdata;
    }

    bool WIsImmutableType(const WObj* obj) {
        return obj->type == WObj::Type::Null
            || obj->type == WObj::Type::Bool
            || obj->type == WObj::Type::Int
            || obj->type == WObj::Type::Float
            || obj->type == WObj::Type::String;
    }

    bool WGetBool(const WObj* obj) {
        WASSERT(obj && WIsBool(obj));
        return obj->b;
    }

    wint WGetInt(const WObj* obj) {
        WASSERT(obj && WIsInt(obj));
        return obj->i;
    }

    wfloat WGetFloat(const WObj* obj) {
        WASSERT(obj && WIsIntOrFloat(obj));
        switch (obj->type) {
        case WObj::Type::Int: return (wfloat)obj->i;
        case WObj::Type::Float: return obj->f;
        default: WUNREACHABLE();
        }
    }

    const char* WGetString(const WObj* obj) {
        WASSERT(obj && WIsString(obj));
        return obj->s.c_str();
    }

    void WGetFunc(const WObj* obj, WFunc* fn) {
        WASSERT(obj && fn && WIsFunc(obj));
        *fn = obj->fn;
    }

    void* WGetUserdata(const WObj* obj) {
        WASSERT(obj && WIsUserdata(obj));
        return obj->u;
    }

	void WGetFinalizer(const WObj* obj, WFinalizer* finalizer) {
		WASSERT(obj && finalizer);
		*finalizer = obj->finalizer;
	}

	void WSetFinalizer(WObj* obj, const WFinalizer* finalizer) {
		WASSERT(obj && finalizer);
		obj->finalizer = *finalizer;
	}

    WObj* WGetAttribute(WObj* obj, const char* member) {
        WASSERT(obj && member);
        WObj* mem = obj->attributes.Get(member);
        if (mem && WIsFunc(mem) && mem->fn.isMethod) {
            mem->self = obj;
        }
        return mem;
    }

    void WSetAttribute(WObj* obj, const char* member, WObj* value) {
        WASSERT(obj && member && value);
        obj->attributes.Set(member, value);
    }

    bool WIterate(WObj* obj, void* userdata, bool(*callback)(WObj*, void*)) {
        WASSERT(obj && callback);
        WObj* iter = WCallMethod(obj, "__iter__", nullptr, 0);
        if (iter == nullptr) {
            WRaiseError(obj->context, "Object is not iterable (does not implement the __iter__ method)");
            return false;
        }
        WProtectObject(iter);

        while (true) {
            WObj* endReached = WCallMethod(iter, "__end__", nullptr, 0);
            if (endReached == nullptr) {
                WRaiseError(obj->context, "Iterator does not implement the __end__ method");
                WUnprotectObject(iter);
                return false;
            }

            WObj* truthy = WTruthy(endReached);
            if (truthy == nullptr) {
                WUnprotectObject(iter);
                return false;
            } else if (WGetBool(truthy)) {
                WUnprotectObject(iter);
                return true;
            }

            WObj* value = WCallMethod(iter, "__next__", nullptr, 0);
            if (endReached == nullptr) {
                WRaiseError(obj->context, "Iterator does not implement the __next__ method");
                WUnprotectObject(iter);
                return false;
            }

            if (!callback(value, userdata)) {
                WUnprotectObject(iter);
                return false;
            }
        }
    }

    WObj* WTruthy(WObj* arg) {
        if (WObj* res = WCallMethod(arg, "__nonzero__", nullptr, 0)) {
            if (WIsBool(res)) {
                return res;
            } else {
                WRaiseError(arg->context, "__nonzero__() returned a non bool type");
            }
        }
        return nullptr;
    }

    WObj* WCastToInt(WObj* arg) {
        if (WObj* res = WCallMethod(arg, "__int__", nullptr, 0)) {
            if (WIsInt(res)) {
                return res;
            } else {
                WRaiseError(arg->context, "__int__() returned a non int type");
            }
        }
        return nullptr;
    }

    WObj* WCastToFloat(WObj* arg) {
        if (WObj* res = WCallMethod(arg, "__float__", nullptr, 0)) {
            if (WIsIntOrFloat(res)) {
                return res;
            } else {
                WRaiseError(arg->context, "__float__() returned a non float type");
            }
        }
        return nullptr;
    }

    WObj* WCastToString(WObj* arg) {
        if (WObj* res = WCallMethod(arg, "__str__", nullptr, 0)) {
            if (WIsString(res)) {
                return res;
            } else {
                WRaiseError(arg->context, "__str__() returned a non str type");
            }
        }
        return nullptr;
    }

    WObj* WCall(WObj* callable, WObj** argv, int argc) {
        WASSERT(callable && argc >= 0 && (argc == 0 || argv));
        if (WIsFunc(callable) || WIsClass(callable)) {
            if (argc)
                WASSERT(argv);
            for (int i = 0; i < argc; i++)
                WASSERT(argv[i]);

            WObj* ret;
            WProtectObject(callable);
            if (callable->self) {
                std::vector<WObj*> argsWithSelf = { callable->self };
                argsWithSelf.insert(argsWithSelf.end(), argv, argv + argc);
                ret = callable->fn.fptr(argsWithSelf.data(), argc + 1, callable->fn.userdata);
            } else {
                ret = callable->fn.fptr(argv, argc, callable->fn.userdata);
            }
            WUnprotectObject(callable);

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
            return WCallMethod(callable, "__call__", argv, argc);
        }
    }

    WObj* WCallMethod(WObj* obj, const char* member, WObj** argv, int argc) {
        WASSERT(obj && member);
        if (argc)
            WASSERT(argv);
        for (int i = 0; i < argc; i++)
            WASSERT(argv[i]);

        WObj* method = WGetAttribute(obj, member);
        if (method == nullptr) {
            std::string msg = "Object of type " +
                WObjTypeToString(obj->type) +
                " has no attribute " +
                member;
            WRaiseError(obj->context, msg.c_str());
            return nullptr;
        } else {
            return WCall(method, argv, argc);
        }
    }

    WObj* WGetIndex(WObj* obj, WObj* index) {
        return WCallMethod(obj, "__getitem__", &index, 1);
    }

    WObj* WSetIndex(WObj* obj, WObj* index, WObj* value) {
        WObj* argv[2] = { index, value };
        return WCallMethod(obj, "__setitem__", argv, 2);
    }

    WObj* WPositive(WObj* arg) {
        return WCallMethod(arg, "__pos__", nullptr, 0);
    }

    WObj* WNegative(WObj* arg) {
        return WCallMethod(arg, "__neg__", nullptr, 0);
    }

    WObj* WAdd(WObj* lhs, WObj* rhs) {
        return WCallMethod(lhs, "__add__", &rhs, 1);
    }

    WObj* WSubtract(WObj* lhs, WObj* rhs) {
        return WCallMethod(lhs, "__sub__", &rhs, 1);
    }

    WObj* WMultiply(WObj* lhs, WObj* rhs) {
        return WCallMethod(lhs, "__mul__", &rhs, 1);
    }

    WObj* WDivide(WObj* lhs, WObj* rhs) {
        return WCallMethod(lhs, "__div__", &rhs, 1);
    }

    WObj* WFloorDivide(WObj* lhs, WObj* rhs) {
        return WCallMethod(lhs, "__floordiv__", &rhs, 1);
    }

    WObj* WModulo(WObj* lhs, WObj* rhs) {
        return WCallMethod(lhs, "__mod__", &rhs, 1);
    }

    WObj* WPower(WObj* lhs, WObj* rhs) {
        return WCallMethod(lhs, "__pow__", &rhs, 1);
    }

    WObj* WEquals(WObj* lhs, WObj* rhs) {
        if (WObj* res = WCallMethod(lhs, "__eq__", &rhs, 1)) {
            if (WIsBool(res)) {
                return res;
            } else {
                WRaiseError(lhs->context, "__eq__() returned a non bool type");
            }
        }
        return nullptr;
    }

    WObj* WNotEquals(WObj* lhs, WObj* rhs) {
        if (WObj* res = WCallMethod(lhs, "__ne__", &rhs, 1)) {
            if (WIsBool(res)) {
                return res;
            } else {
                WRaiseError(lhs->context, "__ne__() returned a non bool type");
            }
        }
        return nullptr;
    }

    WObj* WLessThan(WObj* lhs, WObj* rhs) {
        if (WObj* res = WCallMethod(lhs, "__lt__", &rhs, 1)) {
            if (WIsBool(res)) {
                return res;
            } else {
                WRaiseError(lhs->context, "__lt__() returned a non bool type");
            }
        }
        return nullptr;
    }

    WObj* WLessThanOrEqual(WObj* lhs, WObj* rhs) {
        if (WObj* res = WCallMethod(lhs, "__le__", &rhs, 1)) {
            if (WIsBool(res)) {
                return res;
            } else {
                WRaiseError(lhs->context, "__le__() returned a non bool type");
            }
        }
        return nullptr;
    }

    WObj* WGreaterThan(WObj* lhs, WObj* rhs) {
        if (WObj* res = WCallMethod(lhs, "__gt__", &rhs, 1)) {
            if (WIsBool(res)) {
                return res;
            } else {
                WRaiseError(lhs->context, "__gt__() returned a non bool type");
            }
        }
        return nullptr;
    }

    WObj* WGreaterThanOrEqual(WObj* lhs, WObj* rhs) {
        if (WObj* res = WCallMethod(lhs, "__ge__", &rhs, 1)) {
            if (WIsBool(res)) {
                return res;
            } else {
                WRaiseError(lhs->context, "__ge__() returned a non bool type");
            }
        }
        return nullptr;
    }

    WObj* WIn(WObj* container, WObj* obj) {
        if (WObj* res = WCallMethod(container, "__contains__", &obj, 1)) {
            if (WIsBool(res)) {
                return res;
            } else {
                WRaiseError(container->context, "__contains__() returned a non bool type");
            }
        }
        return nullptr;
    }

    WObj* WNotIn(WObj* container, WObj* obj) {
        if (WObj* inOp = WIn(container, obj)) {
            return WCreateBool(container->context, !WGetBool(inOp));
        } else {
            return nullptr;
        }
    }

    WObj* WBitAnd(WObj* lhs, WObj* rhs) {
        return WCallMethod(lhs, "__and__", &rhs, 1);
    }

    WObj* WBitOr(WObj* lhs, WObj* rhs) {
        return WCallMethod(lhs, "__or__", &rhs, 1);
    }

    WObj* WBitNot(WObj* arg) {
        return WCallMethod(arg, "__invert__", nullptr, 0);
    }

    WObj* WBitXor(WObj* lhs, WObj* rhs) {
        return WCallMethod(lhs, "__xor__", &rhs, 1);
    }

    WObj* WShiftLeft(WObj* lhs, WObj* rhs) {
        return WCallMethod(lhs, "__lshift__", &rhs, 1);
    }

    WObj* WShiftRight(WObj* lhs, WObj* rhs) {
        return WCallMethod(lhs, "__rshift__", &rhs, 1);
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
