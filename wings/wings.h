#pragma once
#include <stdint.h>
#include <vector>
#ifndef __cplusplus
#include <stdbool.h>
#endif

struct WContext;
struct WObj;
typedef int32_t wint;
typedef uint32_t wuint;
typedef float wfloat;

struct WFunc {
    WObj* (*fptr)(WObj** args, int argc, void* userdata);
    void* userdata;
    const char* prettyName;
    bool isMethod;
};

struct WClass {
    WObj** methods;
    const char** methodNames;
    int methodCount;
    const char* prettyName;
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

struct WConfig {
    int maxAlloc;
    int maxRecursion;
    int maxCollectionSize;
    float gcRunFactor;
    void (*print)(const char* message, int len);
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

WDLL_EXPORT WError WGetErrorCode(WContext* context);
WDLL_EXPORT const char* WGetErrorMessage(WContext* context);
WDLL_EXPORT void WRaiseError(WContext* context, const char* message);

WDLL_EXPORT bool WCreateContext(WContext** context, const WConfig* config WDEFAULT_ARG(nullptr));
WDLL_EXPORT void WDestroyContext(WContext* context);
WDLL_EXPORT WObj* WCompile(WContext* context, const char* code, const char* moduleName WDEFAULT_ARG(nullptr));
WDLL_EXPORT void WGetConfig(const WContext* context, WConfig* config);
WDLL_EXPORT void WSetConfig(WContext* context, const WConfig* config);
WDLL_EXPORT void WPrint(const WContext* context, const char* message, int len);
WDLL_EXPORT void WPrintString(const WContext* context, const char* message);

WDLL_EXPORT void WCollectGarbage(WContext* context);
WDLL_EXPORT void WProtectObject(const WObj* obj);
WDLL_EXPORT void WUnprotectObject(const WObj* obj);
WDLL_EXPORT void WLinkReference(WObj* parent, WObj* child);
WDLL_EXPORT void WUnlinkReference(WObj* parent, WObj* child);

WDLL_EXPORT WObj* WGetGlobal(WContext* context, const char* name);
WDLL_EXPORT void WSetGlobal(WContext* context, const char* name, WObj* value);

WDLL_EXPORT WObj* WCreateNoneType(WContext* context);
WDLL_EXPORT WObj* WCreateBool(WContext* context, bool value WDEFAULT_ARG(false));
WDLL_EXPORT WObj* WCreateInt(WContext* context, wint value WDEFAULT_ARG(0));
WDLL_EXPORT WObj* WCreateFloat(WContext* context, wfloat value WDEFAULT_ARG(0));
WDLL_EXPORT WObj* WCreateString(WContext* context, const char* value WDEFAULT_ARG(nullptr));
WDLL_EXPORT WObj* WCreateList(WContext* context);
WDLL_EXPORT WObj* WCreateMap(WContext* context);
WDLL_EXPORT WObj* WCreateFunction(WContext* context, const WFunc* value);
WDLL_EXPORT WObj* WCreateObject(WContext* context);
WDLL_EXPORT WObj* WCreateClass(WContext* context, const WClass* value);
WDLL_EXPORT WObj* WCreateUserdata(WContext* context, void* value);

WDLL_EXPORT bool WIsNoneType(const WObj* obj);
WDLL_EXPORT bool WIsBool(const WObj* obj);
WDLL_EXPORT bool WIsInt(const WObj* obj);
WDLL_EXPORT bool WIsIntOrFloat(const WObj* obj);
WDLL_EXPORT bool WIsString(const WObj* obj);
WDLL_EXPORT bool WIsList(const WObj* obj);
WDLL_EXPORT bool WIsMap(const WObj* obj);
WDLL_EXPORT bool WIsFunc(const WObj* obj);
WDLL_EXPORT bool WIsObject(const WObj* obj);
WDLL_EXPORT bool WIsClass(const WObj* obj);
WDLL_EXPORT bool WIsUserdata(const WObj* obj);
WDLL_EXPORT bool WIsImmutableType(const WObj* obj);

WDLL_EXPORT bool WGetBool(const WObj* obj);
WDLL_EXPORT wint WGetInt(const WObj* obj);
WDLL_EXPORT wfloat WGetFloat(const WObj* obj);
WDLL_EXPORT const char* WGetString(const WObj* obj);
WDLL_EXPORT void WGetFunc(const WObj* obj, WFunc* fn);
WDLL_EXPORT void* WGetUserdata(const WObj* obj);

WDLL_EXPORT void WGetFinalizer(const WObj* obj, WFinalizer* finalizer);
WDLL_EXPORT void WSetFinalizer(WObj* obj, const WFinalizer* finalizer);

//WDLL_EXPORT bool WObjIn(const WObj* container, const WObj* value);
//WDLL_EXPORT bool WObjTruthy(const WObj* obj);
//WDLL_EXPORT bool WObjEquals(const WObj* lhs, const WObj* rhs);
//WDLL_EXPORT int WObjLen(const WObj* obj);
//WDLL_EXPORT WObj* WObjCall(const WObj* func, WObj** args, int argc);
//
//WDLL_EXPORT WObj* WObjListGet(const WObj* list, int index);
//WDLL_EXPORT void WObjListSet(WObj* list, int index, WObj* value);
//WDLL_EXPORT void WObjListPush(WObj* list, WObj* value);
//WDLL_EXPORT void WObjListPop(WObj* list);
//WDLL_EXPORT void WObjListInsert(WObj* list, int index, WObj* value);
//WDLL_EXPORT void WObjListRemoveAt(WObj* list, int index);
//WDLL_EXPORT WObj* WObjMapGet(WObj* map, const WObj* key);
//WDLL_EXPORT void WObjMapSet(WObj* map, const WObj* key, WObj* value);
//WDLL_EXPORT void WObjMapRemove(WObj* map, const WObj* key);

WDLL_EXPORT WObj* WGetAttribute(WObj* obj, const char* member);
WDLL_EXPORT void WSetAttribute(WObj* obj, const char* member, WObj* value);

WDLL_EXPORT bool WIterate(WObj* obj, void* userdata, bool(*callback)(WObj*, void*));

WDLL_EXPORT WObj* WTruthy(WObj* arg);
WDLL_EXPORT WObj* WCastToInt(WObj* arg);
WDLL_EXPORT WObj* WCastToFloat(WObj* arg);
WDLL_EXPORT WObj* WCastToString(WObj* arg);
WDLL_EXPORT WObj* WCall(WObj* callable, WObj** argv, int argc);
WDLL_EXPORT WObj* WCallMethod(WObj* obj, const char* member, WObj** argv, int argc);
WDLL_EXPORT WObj* WGetIndex(WObj* obj, WObj* index);
WDLL_EXPORT WObj* WSetIndex(WObj* obj, WObj* index, WObj* value);
WDLL_EXPORT WObj* WPositive(WObj* arg);
WDLL_EXPORT WObj* WNegative(WObj* arg);
WDLL_EXPORT WObj* WAdd(WObj* lhs, WObj* rhs);
WDLL_EXPORT WObj* WSubtract(WObj* lhs, WObj* rhs);
WDLL_EXPORT WObj* WMultiply(WObj* lhs, WObj* rhs);
WDLL_EXPORT WObj* WDivide(WObj* lhs, WObj* rhs);
WDLL_EXPORT WObj* WFloorDivide(WObj* lhs, WObj* rhs);
WDLL_EXPORT WObj* WModulo(WObj* lhs, WObj* rhs);
WDLL_EXPORT WObj* WPower(WObj* lhs, WObj* rhs);
WDLL_EXPORT WObj* WEquals(WObj* lhs, WObj* rhs);
WDLL_EXPORT WObj* WNotEquals(WObj* lhs, WObj* rhs);
WDLL_EXPORT WObj* WLessThan(WObj* lhs, WObj* rhs);
WDLL_EXPORT WObj* WLessThanOrEqual(WObj* lhs, WObj* rhs);
WDLL_EXPORT WObj* WGreaterThan(WObj* lhs, WObj* rhs);
WDLL_EXPORT WObj* WGreaterThanOrEqual(WObj* lhs, WObj* rhs);
WDLL_EXPORT WObj* WIn(WObj* container, WObj* obj);
WDLL_EXPORT WObj* WNotIn(WObj* container, WObj* obj);
WDLL_EXPORT WObj* WBitAnd(WObj* lhs, WObj* rhs);
WDLL_EXPORT WObj* WBitOr(WObj* lhs, WObj* rhs);
WDLL_EXPORT WObj* WBitNot(WObj* arg);
WDLL_EXPORT WObj* WBitXor(WObj* lhs, WObj* rhs);
WDLL_EXPORT WObj* WShiftLeft(WObj* lhs, WObj* rhs);
WDLL_EXPORT WObj* WShiftRight(WObj* lhs, WObj* rhs);

#undef WDEFAULT_ARG
#undef WDLL_EXPORT

#ifdef __cplusplus
} // extern "C"
#endif
