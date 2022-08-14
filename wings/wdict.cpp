#include "wdict.h"
#include "impl.h"

namespace wings {
	size_t WObjHasher::operator()(const WObj* obj) const {
        auto doHash = []<typename T>(const T & val) { return std::hash<T>()(val); };
        auto rotate = [](size_t x, size_t shift) { return (x << shift) | (x >> (sizeof(size_t) - shift)); };

        if (WIsNone(obj)) {
            return doHash(nullptr);
        } else if (WIsBool(obj)) {
            return doHash(WGetBool(obj));
        } else if (WIsInt(obj)) {
            return doHash(WGetInt(obj));
        } else if (WIsIntOrFloat(obj)) {
            return doHash(WGetFloat(obj));
        } else if (WIsString(obj)) {
            return doHash(obj->Get<std::string>());
        } else if (WIsTuple(obj)) {
            size_t h = 0;
            for (size_t i = 0; i < obj->Get<std::vector<WObj*>>().size(); i++)
                h ^= rotate(doHash(obj->Get<std::vector<WObj*>>()[i]), i);
            return h;
        } else {
            WUNREACHABLE();
        }
	}

    bool WObjComparer::operator()(const WObj* lhs, const WObj* rhs) const {
        if (lhs->type != rhs->type)
            return false;

        if (WIsNone(lhs)) {
            return true;
        } else if (WIsBool(lhs)) {
            return WGetBool(lhs) == WGetBool(rhs);
        } else if (WIsInt(lhs)) {
            return WGetInt(lhs) == WGetInt(rhs);
        } else if (WIsIntOrFloat(lhs)) {
            return WGetFloat(lhs) == WGetFloat(rhs);
        } else if (WIsString(lhs)) {
            return lhs->Get<std::string>() == rhs->Get<std::string>();
        } else {
            return lhs == rhs;
        }
    }
}
