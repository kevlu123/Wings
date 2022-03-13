#pragma once
#include "wings.h"
#include <string>
#include <vector>
#include <deque>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <cstdlib> // std::abort

namespace std {
    template <> struct hash<wings::WObj> {
        size_t operator()(const wings::WObj& obj) const;
    };
}

namespace wings {

    extern thread_local Error werror;

    bool operator==(const WObj& lhs, const WObj& rhs);
    bool operator!=(const WObj& lhs, const WObj& rhs);

    struct WObj {
        enum class Type {
            Null,
            Bool,
            Int,
            Float,
            String,
            List,
            Map,
            Func,
            Userdata,
        } type = Type::Null;

        union {
            bool b;
            int i;
            wfloat f;
            void* u;
            Func fn;
        };
        std::string s;
        std::vector<WObj*> v;
        std::unordered_map<WObj, WObj*> m;
        Finalizer finalizer{};
    };

    struct WContext {
        Config config{};
        std::deque<std::unique_ptr<WObj>> mem;
        std::unordered_multiset<const WObj*> protectedObjects;
    };

} // namespace wings

#define WASSERT(assertion) do { if (!(assertion)) std::abort(); } while (0)
#define WUNREACHABLE() std::abort()
