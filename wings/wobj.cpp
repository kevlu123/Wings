#include "impl.h"
#include "gc.h"
#include <algorithm>

namespace wings {

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

        void WObjGetFunc(const WObj* obj, Func* fn) {
            WASSERT(obj && fn && WObjIsFunc(obj));
            *fn = obj->fn;
        }

        void* WObjGetUserdata(const WObj* obj) {
            WASSERT(obj && WObjIsUserdata(obj));
            return obj->u;
        }

		void WObjGetFinalizer(const WObj* obj, Finalizer* finalizer) {
			WASSERT(obj && finalizer);
			*finalizer = obj->finalizer;
		}

		void WObjSetFinalizer(WObj* obj, const Finalizer* finalizer) {
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
                return std::any_of(
                    container->m.begin(),
                    container->m.end(),
                    [value](const std::pair<const WObj, WObj*>& pair) { return WObjEquals(&pair.first, value); }
                );
            default:
                return false;
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

	} // extern "C"

} // namespace wings

namespace std {
    size_t hash<wings::WObj>::operator()(const wings::WObj& obj) {
        auto doHash = []<typename T>(const T & val) { return std::hash<T>()(val); };

        switch (obj.type) {
        case wings::WObj::Type::Null:     return doHash(nullptr);
        case wings::WObj::Type::Bool:     return doHash(obj.b);
        case wings::WObj::Type::Int:      return doHash(obj.i);
        case wings::WObj::Type::Float:    return doHash(obj.f);
        case wings::WObj::Type::String:   return doHash(obj.s);
        default: WUNREACHABLE();
        }
    }
}
