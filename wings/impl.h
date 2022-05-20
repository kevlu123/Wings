#pragma once
#include "rcptr.h"
#include "wings.h"
#include "attributetable.h"
#include <string>
#include <vector>
#include <deque>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <optional>
#include <cstdlib> // std::abort

namespace std {
    template <> struct hash<WObj> {
        size_t operator()(const WObj& obj) const;
    };
}

namespace wings {

    inline thread_local WError werror;
    inline thread_local std::string werrorMessage;

    inline WObj* listClass;

    size_t Guid();
    bool InitLibrary(WContext* context);
}

bool operator==(const WObj& lhs, const WObj& rhs);
bool operator!=(const WObj& lhs, const WObj& rhs);

struct WContext;

struct WObj {
    enum class Type {
        Null,
        Bool,
        Int,
        Float,
        String,
        List,
        Map,
        Object,
        Func,
        Userdata,
    } type = Type::Null;

    union {
        bool b;
        int i;
        wfloat f;
        void* u;
        struct {
            WObj* self;
            WFunc fn;
        };
    };
    std::string s;
    std::vector<WObj*> v;
    std::unordered_map<WObj, WObj*> m;

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

    struct {
        wings::AttributeTable null;
        wings::AttributeTable _bool;
        wings::AttributeTable _int;
        wings::AttributeTable _float;
        wings::AttributeTable str;
        wings::AttributeTable list;
        wings::AttributeTable map;
        wings::AttributeTable object;
        wings::AttributeTable func;
        wings::AttributeTable userdata;
    } attributeTables;
};

#define WASSERT(assertion) do { if (!(assertion)) std::abort(); } while (0)
#define WUNREACHABLE() std::abort()
