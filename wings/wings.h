#pragma once
#include <stdint.h>
#include <vector>

namespace wings {

    struct WContext;
    struct WObj;
    using wfloat = float;

    struct Func {
        WObj* (*fptr)(WObj** args, int argc, void* userdata);
        void* userdata;
        std::vector<const WObj*> captures;
    };

    struct Finalizer {
        void (*fptr)(WObj* obj, void* userdata);
        void* userdata;
    };

    enum class Error {
        // No error
        Ok,
        // Max wobj allocations reached
        MaxAlloc,
        // Max recursion reached
        MaxRecursion,
    };

    using LogFn = void (*)(const char* message);

    struct Config {
        int maxAlloc;
        int maxRecursion;
        LogFn log;
    };

    extern "C" {

#ifdef _WIN32
#define WDLL_EXPORT __declspec(dllexport)
#else
#define WDLL_EXPORT
#endif

        WDLL_EXPORT Error WErrorGet();
        WDLL_EXPORT WContext* WContextCreate(const Config* config = nullptr);
        WDLL_EXPORT void WContextDestroy(WContext* context);
        WDLL_EXPORT WObj* WContextCompile(WContext* context, const char* code);
        WDLL_EXPORT void WContextGetConfig(const WContext* context, Config* config);
        WDLL_EXPORT void WContextSetConfig(WContext* context, const Config* config);
        WDLL_EXPORT void WContextLog(const WContext* context, const char* message);

        WDLL_EXPORT void WGCProtect(WContext* context, const WObj* obj);
        WDLL_EXPORT void WGCUnprotect(WContext* context, const WObj* obj);
        WDLL_EXPORT void WGCCollect(WContext* context);

        WDLL_EXPORT WObj* WObjCreateNull(WContext* context);
        WDLL_EXPORT WObj* WObjCreateBool(WContext* context, bool value = false);
        WDLL_EXPORT WObj* WObjCreateInt(WContext* context, int value = 0);
        WDLL_EXPORT WObj* WObjCreateFloat(WContext* context, wfloat value = 0);
        WDLL_EXPORT WObj* WObjCreateString(WContext* context, const char* value = "");
        WDLL_EXPORT WObj* WObjCreateList(WContext* context);
        WDLL_EXPORT WObj* WObjCreateMap(WContext* context);
        WDLL_EXPORT WObj* WObjCreateFunc(WContext* context, const Func* value);
        WDLL_EXPORT WObj* WObjCreateUserdata(WContext* context, void* value);

        WDLL_EXPORT bool WObjIsNull(const WObj* obj);
        WDLL_EXPORT bool WObjIsBool(const WObj* obj);
        WDLL_EXPORT bool WObjIsInt(const WObj* obj);
        WDLL_EXPORT bool WObjIsIntOrFloat(const WObj* obj);
        WDLL_EXPORT bool WObjIsString(const WObj* obj);
        WDLL_EXPORT bool WObjIsList(const WObj* obj);
        WDLL_EXPORT bool WObjIsMap(const WObj* obj);
        WDLL_EXPORT bool WObjIsFunc(const WObj* obj);
        WDLL_EXPORT bool WObjIsUserdata(const WObj* obj);

        WDLL_EXPORT bool WObjGetBool(const WObj* obj);
        WDLL_EXPORT int WObjGetInt(const WObj* obj);
        WDLL_EXPORT wfloat WObjGetFloat(const WObj* obj);
        WDLL_EXPORT const char* WObjGetString(const WObj* obj);
        WDLL_EXPORT void WObjGetFunc(const WObj* obj, Func* fn);
        WDLL_EXPORT void* WObjGetUserdata(const WObj* obj);

        WDLL_EXPORT bool WObjIn(const WObj* container, const WObj* value);
        WDLL_EXPORT bool WObjTruthy(const WObj* obj);
        WDLL_EXPORT bool WObjEquals(const WObj* lhs, const WObj* rhs);
        WDLL_EXPORT WObj* WObjCall(const WObj* func, WObj** args, int argc);
        WDLL_EXPORT int WObjLen(const WObj* obj);

        WDLL_EXPORT WObj* WObjListGet(WObj* list);
        WDLL_EXPORT void WObjListSet(WObj* list, WObj* value);
        WDLL_EXPORT void WObjListPush(WObj* list, WObj* value);
        WDLL_EXPORT void WObjListPop(WObj* list);
        WDLL_EXPORT void WObjListInsert(WObj* list, int index, WObj* value);
        WDLL_EXPORT void WObjListRemoveAt(WObj* list, int index);
        WDLL_EXPORT void WObjMapSet(WObj* map, const WObj* key, WObj* value);
        WDLL_EXPORT void WObjMapRemove(WObj* map, const WObj* key);

        WDLL_EXPORT void WObjGetFinalizer(const WObj* obj, Finalizer* finalizer);
        WDLL_EXPORT void WObjSetFinalizer(WObj* obj, const Finalizer* finalizer);

#undef WDLL_EXPORT

    } // extern "C"

} // namespace wings
