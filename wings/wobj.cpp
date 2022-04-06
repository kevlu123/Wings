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

extern "C" {

    WObj* WObjCreateNull(WContext* context) {
        WASSERT(context);
        auto obj = Alloc(context);
        obj->type = WObj::Type::Null;
        return obj;
    }

    WObj* WObjCreateBool(WContext* context, bool value) {
        WASSERT(context);
        auto obj = Alloc(context);
        obj->type = WObj::Type::Bool;
        obj->b = value;
        return obj;
    }

    WObj* WObjCreateInt(WContext* context, int value) {
        WASSERT(context);
        auto obj = Alloc(context);
        obj->type = WObj::Type::Int;
        obj->i = value;
        return obj;
    }

    WObj* WObjCreateFloat(WContext* context, wfloat value) {
        WASSERT(context);
        auto obj = Alloc(context);
        obj->type = WObj::Type::Float;
        obj->f = value;
        return obj;
    }

    WObj* WObjCreateString(WContext* context, const char* value) {
        WASSERT(context && value);
        auto obj = Alloc(context);
        obj->type = WObj::Type::String;
        obj->s = value;
        return obj;
    }

    WObj* WObjCreateList(WContext* context) {
        WASSERT(context);
        auto obj = Alloc(context);
        obj->type = WObj::Type::List;
        return obj;
    }

    WObj* WObjCreateMap(WContext* context) {
        WASSERT(context);
        auto obj = Alloc(context);
        obj->type = WObj::Type::Map;
        return obj;
    }

    WObj* WObjCreateFunc(WContext* context, const WFunc* value) {
        WASSERT(context && value && value->fptr);
        auto obj = Alloc(context);
        obj->type = WObj::Type::Func;
        obj->fn = *value;
        return obj;
    }

    WObj* WObjCreateUserdata(WContext* context, void* value) {
        WASSERT(context);
        auto obj = Alloc(context);
        obj->type = WObj::Type::Userdata;
        obj->u = value;
        return obj;
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

    bool WObjIn(const WObj* container, const WObj* value) {
        WASSERT(container && value);
        switch (container->type) {
        case WObj::Type::String:
            return WObjIsString(value)
                && container->s.find(value->s) != std::string::npos;
        case WObj::Type::List:
            return std::any_of(
                container->v.begin(),
                container->v.end(),
                [value](const WObj* obj) { return WObjEquals(obj, value); }
            );
        case WObj::Type::Map:
            return container->m.find(*value) != container->m.end();
        default:
            WUNREACHABLE();
        }
    }

    bool WObjTruthy(const WObj* obj) {
        WASSERT(obj);
        switch (obj->type) {
        case WObj::Type::Null: return false;
        case WObj::Type::Bool: return obj->b;
        case WObj::Type::Int: return obj->i;
        case WObj::Type::Float: return obj->f;
        case WObj::Type::String: return obj->s.size();
        case WObj::Type::List: return obj->v.size();
        case WObj::Type::Map: return obj->m.size();
        case WObj::Type::Func: return true;
        case WObj::Type::Userdata: return true;
        default: WUNREACHABLE();
        }
    }
        
    bool WObjEquals(const WObj* lhs, const WObj* rhs) {
        WASSERT(lhs && rhs);
        return *lhs == *rhs;
    }
        
    int WObjLen(const WObj* obj) {
        WASSERT(obj);
        switch (obj->type) {
        case WObj::Type::String: return (int)obj->s.size();
        case WObj::Type::List: return (int)obj->s.size();
        case WObj::Type::Map: return (int)obj->s.size();
        default: WUNREACHABLE();
        }
    }
        
    WObj* WObjCall(const WObj* func, WObj** args, int argc) {
        WASSERT(func && argc >= 0 && WObjIsFunc(func));
        if (argc)
            WASSERT(args);
        for (int i = 0; i < argc; i++)
            WASSERT(args[i]);
        return func->fn.fptr(args, argc, func->fn.userdata);
    }
        
    WObj* WObjListGet(const WObj* list, int index) {
        WASSERT(list && WObjIsList(list) && index >= 0 && (size_t)index < list->v.size());
        return list->v[index];
    }
        
    void WObjListSet(WObj* list, int index, WObj* value) {
        WASSERT(list && WObjIsList(list) && index >= 0 && (size_t)index < list->v.size());
        list->v[index] = value;
    }
        
    void WObjListPush(WObj* list, WObj* value) {
        WASSERT(list && value && WObjIsList(list));
        WObjListInsert(list, (int)list->v.size(), value);
    }
        
    void WObjListPop(WObj* list) {
        WASSERT(list && WObjIsList(list));
        WObjListRemoveAt(list, (int)list->v.size() - 1);
    }
        
    void WObjListInsert(WObj* list, int index, WObj* value) {
        WASSERT(list && WObjIsList(list) && index >= 0 && (size_t)index <= list->v.size() && value);
        list->v.insert(list->v.begin() + index, value);
    }
        
    void WObjListRemoveAt(WObj* list, int index) {
        WASSERT(list && WObjIsList(list) && index >= 0 && (size_t)index < list->v.size());
        list->v.erase(list->v.begin() + index);
    }
        
    WObj* WObjMapGet(WObj* map, const WObj* key) {
        WASSERT(map && key && WObjIsMap(map) && IsImmutable(key));
        auto it = map->m.find(*key);
        WASSERT(it != map->m.end());
        return it->second;
    }

    void WObjMapSet(WObj* map, const WObj* key, WObj* value) {
        WASSERT(map && key && value && WObjIsMap(map) && IsImmutable(key));
        map->m[*key] = value;
    }

    void WObjMapRemove(WObj* map, const WObj* key) {
        WASSERT(map && key && WObjIsMap(map) && IsImmutable(key));
        auto it = map->m.find(*key);
        WASSERT(it != map->m.end());
        map->m.erase(it);
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
