#pragma once
#include <stdint.h>
#include <vector>
#ifndef __cplusplus
#include <stdbool.h>
#endif

struct WContext;
struct WObj;
typedef float wfloat;

struct WFunc {
    WObj* (*fptr)(WObj** args, int argc, void* userdata);
    void* userdata;
    const char* prettyName;
    bool isMethod;
};

struct WFinalizer {
    void (*fptr)(WObj* obj, void* userdata);
    void* userdata;
};

#ifdef __cplusplus
enum class WError {
#else
enum WError {
#endif
    // No error
    WERROR_OK,
    // Max WObj allocations reached
    WERROR_MAX_ALLOC,
    // Max recursion reached
    WERROR_MAX_RECURSION,
    // Error while compiling code
    WERROR_COMPILE_FAILED,
    // Error while executing user code
    WERROR_RUNTIME_ERROR,
};

typedef void (*WLogFn)(const char* message);

struct WConfig {
    int maxAlloc;
    int maxRecursion;
    int maxCollectionSize;
    float gcRunFactor;
    WLogFn log;
};

#ifdef _WIN32
#define WDLL_EXPORT __declspec(dllexport)
#else
#define WDLL_EXPORT
#endif

#ifdef __cplusplus
#define WDEFAULT_ARG(arg) = arg
#else
#define WDEFAULT_ARG(arg)
#endif

#ifdef __cplusplus
extern "C" {
#endif

WDLL_EXPORT WError WErrorGet();
WDLL_EXPORT const char* WErrorMessageGet();
WDLL_EXPORT void WErrorSetRuntimeError(const char* message);

WDLL_EXPORT WContext* WContextCreate(const WConfig* config WDEFAULT_ARG(nullptr));
WDLL_EXPORT void WContextDestroy(WContext* context);
WDLL_EXPORT WObj* WContextCompile(WContext* context, const char* code);
WDLL_EXPORT void WContextGetConfig(const WContext* context, WConfig* config);
WDLL_EXPORT void WContextSetConfig(WContext* context, const WConfig* config);
WDLL_EXPORT void WContextLog(const WContext* context, const char* message);

WDLL_EXPORT void WGcCollect(WContext* context);
WDLL_EXPORT void WGcProtect(const WObj* obj);
WDLL_EXPORT void WGcUnprotect(const WObj* obj);
WDLL_EXPORT void WGcCreateReference(WObj* parent, WObj* child);
WDLL_EXPORT void WGcRemoveReference(WObj* parent, WObj* child);

WDLL_EXPORT WObj* WContextGetGlobal(WContext* context, const char* name);
WDLL_EXPORT void WContextSetGlobal(WContext* context, const char* name, WObj* value);

WDLL_EXPORT WObj* WObjCreateNull(WContext* context);
WDLL_EXPORT WObj* WObjCreateBool(WContext* context, bool value WDEFAULT_ARG(false));
WDLL_EXPORT WObj* WObjCreateInt(WContext* context, int value WDEFAULT_ARG(0));
WDLL_EXPORT WObj* WObjCreateFloat(WContext* context, wfloat value WDEFAULT_ARG(0));
WDLL_EXPORT WObj* WObjCreateString(WContext* context, const char* value WDEFAULT_ARG(""));
WDLL_EXPORT WObj* WObjCreateList(WContext* context);
WDLL_EXPORT WObj* WObjCreateMap(WContext* context);
WDLL_EXPORT WObj* WObjCreateObject(WContext* context);
WDLL_EXPORT WObj* WObjCreateFunc(WContext* context, const WFunc* value);
WDLL_EXPORT WObj* WObjCreateUserdata(WContext* context, void* value);

WDLL_EXPORT bool WObjIsNull(const WObj* obj);
WDLL_EXPORT bool WObjIsBool(const WObj* obj);
WDLL_EXPORT bool WObjIsInt(const WObj* obj);
WDLL_EXPORT bool WObjIsIntOrFloat(const WObj* obj);
WDLL_EXPORT bool WObjIsString(const WObj* obj);
WDLL_EXPORT bool WObjIsList(const WObj* obj);
WDLL_EXPORT bool WObjIsMap(const WObj* obj);
WDLL_EXPORT bool WObjIsObject(const WObj* obj);
WDLL_EXPORT bool WObjIsFunc(const WObj* obj);
WDLL_EXPORT bool WObjIsUserdata(const WObj* obj);

WDLL_EXPORT bool WObjGetBool(const WObj* obj);
WDLL_EXPORT int WObjGetInt(const WObj* obj);
WDLL_EXPORT wfloat WObjGetFloat(const WObj* obj);
WDLL_EXPORT const char* WObjGetString(const WObj* obj);
WDLL_EXPORT void WObjGetFunc(const WObj* obj, WFunc* fn);
WDLL_EXPORT void* WObjGetUserdata(const WObj* obj);

WDLL_EXPORT bool WObjIn(const WObj* container, const WObj* value);
WDLL_EXPORT bool WObjTruthy(const WObj* obj);
WDLL_EXPORT bool WObjEquals(const WObj* lhs, const WObj* rhs);
WDLL_EXPORT int WObjLen(const WObj* obj);
WDLL_EXPORT WObj* WObjCall(const WObj* func, WObj** args, int argc);

WDLL_EXPORT WObj* WObjListGet(const WObj* list, int index);
WDLL_EXPORT void WObjListSet(WObj* list, int index, WObj* value);
WDLL_EXPORT void WObjListPush(WObj* list, WObj* value);
WDLL_EXPORT void WObjListPop(WObj* list);
WDLL_EXPORT void WObjListInsert(WObj* list, int index, WObj* value);
WDLL_EXPORT void WObjListRemoveAt(WObj* list, int index);
WDLL_EXPORT WObj* WObjMapGet(WObj* map, const WObj* key);
WDLL_EXPORT void WObjMapSet(WObj* map, const WObj* key, WObj* value);
WDLL_EXPORT void WObjMapRemove(WObj* map, const WObj* key);

WDLL_EXPORT WObj* WObjGetAttribute(WObj* obj, const char* member);
WDLL_EXPORT void WObjSetAttribute(WObj* obj, const char* member, WObj* value);

WDLL_EXPORT void WObjGetFinalizer(const WObj* obj, WFinalizer* finalizer);
WDLL_EXPORT void WObjSetFinalizer(WObj* obj, const WFinalizer* finalizer);

#undef WDEFAULT_ARG
#undef WDLL_EXPORT

#ifdef __cplusplus
} // extern "C"
#endif
