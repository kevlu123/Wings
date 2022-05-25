#include "impl.h"
#include "gc.h"
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

static bool IsImmutable(const WObj* obj) {
    return obj->type == WObj::Type::Null
        || obj->type == WObj::Type::Bool
        || obj->type == WObj::Type::Int
        || obj->type == WObj::Type::Float
        || obj->type == WObj::Type::String;
}

static void AppendTraceback(const WObj* func) {
    TraceFrame frame{};
    frame.func = func->fn.prettyName ? func->fn.prettyName : "";
    func->context->err.trace.push_back(std::move(frame));
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

    WObj* WObjCreateInt(WContext* context, int value) {
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

    bool WObjGetBool(const WObj* obj) {
        WASSERT(obj && WObjIsBool(obj));
        return obj->b;
    }

    int WObjGetInt(const WObj* obj) {
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

    //bool WObjIn(const WObj* container, const WObj* value) {
    //    WASSERT(container && value);
    //    switch (container->type) {
    //    case WObj::Type::String:
    //        return WObjIsString(value)
    //            && container->s.find(value->s) != std::string::npos;
    //    case WObj::Type::List:
    //        return std::any_of(
    //            container->v.begin(),
    //            container->v.end(),
    //            [value](const WObj* obj) { return WObjEquals(obj, value); }
    //        );
    //    case WObj::Type::Map:
    //        return container->m.find(*value) != container->m.end();
    //    default:
    //        WUNREACHABLE();
    //    }
    //}
    //
    //bool WObjTruthy(const WObj* obj) {
    //    WASSERT(obj);
    //    switch (obj->type) {
    //    case WObj::Type::Null: return false;
    //    case WObj::Type::Bool: return obj->b;
    //    case WObj::Type::Int: return obj->i;
    //    case WObj::Type::Float: return obj->f;
    //    case WObj::Type::String: return obj->s.size();
    //    case WObj::Type::List: return obj->v.size();
    //    case WObj::Type::Map: return obj->m.size();
    //    case WObj::Type::Func: return true;
    //    case WObj::Type::Userdata: return true;
    //    default: WUNREACHABLE();
    //    }
    //}
    //    
    //bool WObjEquals(const WObj* lhs, const WObj* rhs) {
    //    WASSERT(lhs && rhs);
    //    return *lhs == *rhs;
    //}
    //    
    //int WObjLen(const WObj* obj) {
    //    WASSERT(obj);
    //    switch (obj->type) {
    //    case WObj::Type::String: return (int)obj->s.size();
    //    case WObj::Type::List: return (int)obj->s.size();
    //    case WObj::Type::Map: return (int)obj->s.size();
    //    default: WUNREACHABLE();
    //    }
    //}
    //    
    //WObj* WObjCall(const WObj* func, WObj** args, int argc) {
    //    WASSERT(func && argc >= 0 && (WObjIsFunc(func) || WObjIsClass(func)));
    //    if (argc)
    //        WASSERT(args);
    //    for (int i = 0; i < argc; i++)
    //        WASSERT(args[i]);
    //
    //    WObj* ret;
    //    WGcProtect(func);
    //    if (func->self) {
    //        std::vector<WObj*> argsWithSelf = { func->self };
    //        argsWithSelf.insert(argsWithSelf.end(), args, args + argc);
    //        ret = func->fn.fptr(argsWithSelf.data(), argc + 1, func->fn.userdata);
    //    } else {
    //        ret = func->fn.fptr(args, argc, func->fn.userdata);
    //    }
    //    WGcUnprotect(func);
    //
    //    if (ret == nullptr) {
    //        AppendTraceback(func);
    //    }
    //
    //    return ret;
    //}
    //    
    //WObj* WObjListGet(const WObj* list, int index) {
    //    WASSERT(list && WObjIsList(list) && index >= 0 && (size_t)index < list->v.size());
    //    return list->v[index];
    //}
    //    
    //void WObjListSet(WObj* list, int index, WObj* value) {
    //    WASSERT(list && WObjIsList(list) && index >= 0 && (size_t)index < list->v.size());
    //    list->v[index] = value;
    //}
    //    
    //void WObjListPush(WObj* list, WObj* value) {
    //    WASSERT(list && value && WObjIsList(list));
    //    WObjListInsert(list, (int)list->v.size(), value);
    //}
    //    
    //void WObjListPop(WObj* list) {
    //    WASSERT(list && WObjIsList(list));
    //    WObjListRemoveAt(list, (int)list->v.size() - 1);
    //}
    //    
    //void WObjListInsert(WObj* list, int index, WObj* value) {
    //    WASSERT(list && WObjIsList(list) && index >= 0 && (size_t)index <= list->v.size() && value);
    //    list->v.insert(list->v.begin() + index, value);
    //}
    //    
    //void WObjListRemoveAt(WObj* list, int index) {
    //    WASSERT(list && WObjIsList(list) && index >= 0 && (size_t)index < list->v.size());
    //    list->v.erase(list->v.begin() + index);
    //}
    //    
    //WObj* WObjMapGet(WObj* map, const WObj* key) {
    //    WASSERT(map && key && WObjIsMap(map) && IsImmutable(key));
    //    auto it = map->m.find(*key);
    //    WASSERT(it != map->m.end());
    //    return it->second;
    //}
    //
    //void WObjMapSet(WObj* map, const WObj* key, WObj* value) {
    //    WASSERT(map && key && value && WObjIsMap(map) && IsImmutable(key));
    //    map->m[*key] = value;
    //}
    //
    //void WObjMapRemove(WObj* map, const WObj* key) {
    //    WASSERT(map && key && WObjIsMap(map) && IsImmutable(key));
    //    auto it = map->m.find(*key);
    //    WASSERT(it != map->m.end());
    //    map->m.erase(it);
    //}

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

    WObj* WOpTruthy(WObj* arg) {
        WASSERT(arg);
        if (WObj* res = WOpCallMethod(arg, "__nonzero__", nullptr, 0)) {
            if (WObjIsBool(res)) {
                return res;
            } else {
                WErrorSetRuntimeError(arg->context, "__nonzero__() returned a non bool type");
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

            if (ret == nullptr) {
                AppendTraceback(callable);
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
            std::string msg = "object of type " +
                WObjTypeToString(obj->type) +
                " has no attribute " +
                member;
            WErrorSetRuntimeError(obj->context, msg.c_str());
            return nullptr;
        } else {
            return WOpCall(method, argv, argc);
        }
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

    WObj* WOpIn(WObj* lhs, WObj* rhs) {
        if (WObj* res = WOpCallMethod(lhs, "__contains__", &rhs, 1)) {
            if (WObjIsBool(res)) {
                return res;
            } else {
                WErrorSetRuntimeError(lhs->context, "__contains__() returned a non bool type");
            }
        }
        return nullptr;
    }

    WObj* WOpNotIn(WObj* lhs, WObj* rhs) {
        if (WObj* inOp = WOpIn(lhs, rhs)) {
            return WObjCreateBool(lhs->context, !WObjGetBool(inOp));
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
