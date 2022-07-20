#include "wdict.h"
#include "impl.h"

namespace wings {
	size_t WObjHasher::operator()(const WObj* obj) const {
        auto doHash = []<typename T>(const T & val) { return std::hash<T>()(val); };
        auto rotate = [](size_t x, size_t shift) { return (x << shift) | (x >> (sizeof(size_t) - shift)); };

        switch (obj->type) {
        case WObj::Type::Null:     return doHash(nullptr);
        case WObj::Type::Bool:     return doHash(obj->b);
        case WObj::Type::Int:      return doHash(obj->i);
        case WObj::Type::Float:    return doHash(obj->f);
        case WObj::Type::String:   return doHash(obj->s);
        case WObj::Type::Tuple: {
            size_t h = 0;
            for (size_t i = 0; i < obj->v.size(); i++)
                h ^= rotate(doHash(obj->v[i]), i);
            return h;
        }
        default: WUNREACHABLE();
        }
	}

    bool WObjComparer::operator()(const WObj* lhs, const WObj* rhs) const {
        if (lhs->type != rhs->type)
            return false;

        switch (lhs->type) {
        case WObj::Type::Null:   return true;
        case WObj::Type::Bool:   return lhs->b == rhs->b;
        case WObj::Type::Int:    return lhs->i == rhs->i;
        case WObj::Type::Float:  return lhs->f == rhs->f;
        case WObj::Type::String: return lhs->s == rhs->s;
        case WObj::Type::List:
        case WObj::Type::Map:
        case WObj::Type::Func:
        case WObj::Type::Userdata: return lhs == rhs;
        default: WUNREACHABLE();
        }
    }
}
