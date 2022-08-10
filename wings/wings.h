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

struct WFuncDesc {
    WObj* (*fptr)(WObj** args, int argc, WObj* kwargs, void* userdata);
    void* userdata;
    bool isMethod;
    const char* prettyName;
};

struct WFinalizer {
    void (*fptr)(WObj* obj, void* userdata);
    void* userdata;
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

/**
* Create a context (an instance of an interpreter).
* 
* If the function succeeds, the returned context must be freed with WDestroyContext().
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
* 
* @param config a pointer to a WConfig struct containing configuration information,
*               or nullptr for the default configuration.
* @return a valid WContext* on success, or nullptr on failure.
*/
WDLL_EXPORT WContext* WCreateContext(const WConfig* config WDEFAULT_ARG(nullptr));

/**
* Free a context created with WCreateContext().
* 
* This function can be called with nullptr.
* 
* @param context the context to free.
*/
WDLL_EXPORT void WDestroyContext(WContext* context);

/**
* Compile a script.
* 
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
* 
* @param context the relevant context.
* @param code a null terminated ASCII string containing the source code.
* @param moduleName an optional null terminated ASCII string containing the
*                   name used for generating error messages relating to this script.
* @return the compiled function object, or nullptr on failure. Call WCall() to execute the function object.
*/
WDLL_EXPORT WObj* WCompile(WContext* context, const char* code, const char* moduleName WDEFAULT_ARG(nullptr));

/**
* Get the current error as a string.
* 
* @param context the relevant context.
* @return the current error as a null terminated ASCII string. This string is owned by the function and should not be freed.
*/
WDLL_EXPORT const char* WGetErrorMessage(WContext* context);

/**
* Get the current exception.
*
* @param context the relevant context.
* @return the current exception object, or nullptr if there is no exception.
*/
WDLL_EXPORT WObj* WGetCurrentException(WContext* context);

/**
* Set a runtime error.
* 
* @param context the relevant context.
* @param message a null terminated ASCII string containing the error message string.
*/
WDLL_EXPORT void WRaiseException(WContext* context, const char* message WDEFAULT_ARG(nullptr), WObj* type WDEFAULT_ARG(nullptr));
WDLL_EXPORT void WRaiseExceptionObject(WContext* context, WObj* exception);

WDLL_EXPORT void WRaiseArgumentCountError(WContext* context, int given, int expected);
WDLL_EXPORT void WRaiseArgumentTypeError(WContext* context, int argIndex, const char* expected);
WDLL_EXPORT void WRaiseAttributeError(const WObj* obj, const char* attribute);
WDLL_EXPORT bool WIsInstance(const WObj* instance, const WObj*const* types, int typesLen);

/**
* Clear the current exception.
*
* @param context the relevant context.
*/
WDLL_EXPORT void WClearCurrentException(WContext* context);

/**
* Get the current configuration.
* 
* @param context the relevant context.
* @param config a pointer to a WConfig struct to retrieve the configuration information.
*/
WDLL_EXPORT void WGetConfig(const WContext* context, WConfig* config);

/**
* Set the current configuration.
* 
* @param context the relevant context.
* @param config a pointer to a WConfig struct containing the configuration information.
*/
WDLL_EXPORT void WSetConfig(WContext* context, const WConfig* config);

/**
* Get a global variable.
* 
* @param context the relevant context.
* @param name a null terminated ASCII string containing the name of the global variable to retrieve.
* @return a value of the global variable, or nullptr if it does not exist.
*/
WDLL_EXPORT WObj* WGetGlobal(WContext* context, const char* name);

/**
* Set a global variable.
* 
* @param context the relevant context.
* @param name a null terminated ASCII string containing the name of the global variable to set.
* @param value the value to set the global variable to.
*/
WDLL_EXPORT void WSetGlobal(WContext* context, const char* name, WObj* value);
WDLL_EXPORT void WDeleteGlobal(WContext* context, const char* name);

/**
* Print a message.
* 
* This function uses the print function specified in the current configuration.
* By default this is std::cout.
* 
* @param context the relevant context.
* @param message a pointer to a byte array containing the message.
* @param len the length of the message in bytes.
*/
WDLL_EXPORT void WPrint(const WContext* context, const char* message, int len);

/**
* Print a message.
*
* This function uses the print function specified in the current configuration.
* By default this is std::cout.
*
* @param context the relevant context.
* @param message a pointer to a null terminated ASCII string containing the message.
*/
WDLL_EXPORT void WPrintString(const WContext* context, const char* message);

/**
* Run the garbage collector.
* 
* This frees all unreachable objects.
* 
* @param context the relevant context.
*/
WDLL_EXPORT void WCollectGarbage(WContext* context);

/**
* Protect an object from being garbage collected.
* 
* Call WUnprotectObject() to allow the object to be collected again.
* 
* @param obj the object to protect.
*/
WDLL_EXPORT void WProtectObject(const WObj* obj);

/**
* Allow a object protected with WProtectObject() to be garbage collected again.
*
* @param obj the object to unprotect.
*/
WDLL_EXPORT void WUnprotectObject(const WObj* obj);

/**
* Make a parent object reference a child object.
* 
* This tells the garbage collector to preserve the child object if
* the parent object is still reachable. It is not necessary to
* call WUnlinkReference() after calling this function.
* 
* @param parent the parent object.
* @param child the child object being.
*/
WDLL_EXPORT void WLinkReference(WObj* parent, WObj* child);

/**
* Remove a reference created with WLinkReference().
* 
* @param parent the parent object.
* @param child the child object.
*/
WDLL_EXPORT void WUnlinkReference(WObj* parent, WObj* child);

/**
* Instantiate a None object.
* 
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
* 
* @param context the relevant context.
* @return the instantiated object, or nullptr on failure.
*/
WDLL_EXPORT WObj* WCreateNoneType(WContext* context);

/**
* Instantiate a boolean object.
* 
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
* 
* @param context the relevant context.
* @param value the value of the object.
* @return the instantiated object, or nullptr on failure.
*/
WDLL_EXPORT WObj* WCreateBool(WContext* context, bool value WDEFAULT_ARG(false));

/**
* Instantiate an integer object.
*
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param context the relevant context.
* @param value the value of the object.
* @return the instantiated object, or nullptr on failure.
*/
WDLL_EXPORT WObj* WCreateInt(WContext* context, wint value WDEFAULT_ARG(0));

/**
* Instantiate a float object.
*
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param context the relevant context.
* @param value the value of the object.
* @return the instantiated object, or nullptr on failure.
*/
WDLL_EXPORT WObj* WCreateFloat(WContext* context, wfloat value WDEFAULT_ARG(0));

/**
* Instantiate a string object.
*
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param context the relevant context.
* @param value a null terminated ASCII string, or nullptr for an empty string.
* @return the instantiated object, or nullptr on failure.
*/
WDLL_EXPORT WObj* WCreateString(WContext* context, const char* value WDEFAULT_ARG(nullptr));

/**
* Instantiate a tuple object.
*
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param context the relevant context.
* @param argv a pointer to an array of objects to initialize the tuple with.
*             This can be nullptr if argc is 0.
* @param argc the number of objects to initialize the tuple with.
* @return the instantiated object, or nullptr on failure.
*/
WDLL_EXPORT WObj* WCreateTuple(WContext* context, WObj** argv, int argc);

/**
* Instantiate a list object.
*
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param context the relevant context.
* @param argv a pointer to an array of objects to initialize the list with.
*             This can be nullptr if argc is 0.
* @param argc the number of objects to initialize the list with.
* @return the instantiated object, or nullptr on failure.
*/
WDLL_EXPORT WObj* WCreateList(WContext* context, WObj** argv WDEFAULT_ARG(nullptr), int argc WDEFAULT_ARG(0));

/**
* Instantiate a dictionary object.
*
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param context the relevant context.
* @param keys a pointer to an array of keys to initialize the dictionary with.
*             This can be nullptr if argc is 0.
* @param values a pointer to an array of values to initialize the dictionary with.
*             This can be nullptr if argc is 0.
* @param argc the number of key value pairs to initialize the dictionary with.
* @return the instantiated object, or nullptr on failure.
*/
WDLL_EXPORT WObj* WCreateDictionary(WContext* context, WObj** keys WDEFAULT_ARG(nullptr), WObj** values WDEFAULT_ARG(nullptr), int argc WDEFAULT_ARG(0));

/**
* Instantiate a function object.
*
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param context the relevant context.
* @param value a pointer to a WFunc struct containing the function information.
* @return the instantiated object, or nullptr on failure.
*/
WDLL_EXPORT WObj* WCreateFunction(WContext* context, const WFuncDesc* value);

/**
* Instantiate a class object.
*
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param context the relevant context.
* @param value a pointer to a WClass struct containing the class information.
* @return the instantiated object, or nullptr on failure.
*/
WDLL_EXPORT WObj* WCreateClass(WContext* context, const char* name, WObj** bases, int baseCount);
WDLL_EXPORT void WAddAttributeToClass(WObj* _class, const char* name, WObj* attribute);

/**
* Check if an object is None.
* 
* @param obj the object to inspect.
* @return true if the object is None, otherwise false.
*/
WDLL_EXPORT bool WIsNoneType(const WObj* obj);

/**
* Check if an object is a boolean.
*
* @param obj the object to inspect.
* @return true if the object is a boolean, otherwise false.
*/
WDLL_EXPORT bool WIsBool(const WObj* obj);

/**
* Check if an object is an integer.
*
* @param obj the object to inspect.
* @return true if the object is an integer, otherwise false.
*/
WDLL_EXPORT bool WIsInt(const WObj* obj);

/**
* Check if an object is a float.
*
* @param obj the object to inspect.
* @return true if the object is a float, otherwise false.
*/
WDLL_EXPORT bool WIsIntOrFloat(const WObj* obj);

/**
* Check if an object is a string.
*
* @param obj the object to inspect.
* @return true if the object is a string, otherwise false.
*/
WDLL_EXPORT bool WIsString(const WObj* obj);

/**
* Check if an object is a tuple.
*
* @param obj the object to inspect.
* @return true if the object is a tuple, otherwise false.
*/
WDLL_EXPORT bool WIsTuple(const WObj* obj);

/**
* Check if an object is a list.
*
* @param obj the object to inspect.
* @return true if the object is a list, otherwise false.
*/
WDLL_EXPORT bool WIsList(const WObj* obj);

/**
* Check if an object is a dictionary.
*
* @param obj the object to inspect.
* @return true if the object is a dictionary, otherwise false.
*/
WDLL_EXPORT bool WIsDictionary(const WObj* obj);

/**
* Check if an object is a function.
*
* @param obj the object to inspect.
* @return true if the object is a function, otherwise false.
*/
WDLL_EXPORT bool WIsFunction(const WObj* obj);

/**
* Check if an object is a class.
*
* @param obj the object to inspect.
* @return true if the object is a class, otherwise false.
*/
WDLL_EXPORT bool WIsClass(const WObj* obj);

/**
* Check if an object is an immutable type.
*
* Immutable types are None, bool, int, float, str, and tuples with immutable elements.
* 
* @param obj the object to inspect.
* @return true if the object is an immutable type, otherwise false.
*/
WDLL_EXPORT bool WIsImmutableType(const WObj* obj);

/**
* Get the value from a boolean object.
* 
* @param obj the object to get the value from.
* @return the boolean value of the object.
*/
WDLL_EXPORT bool WGetBool(const WObj* obj);

/**
* Get the value from an integer object.
*
* @param obj the object to get the value from.
* @return the integer value of the object.
*/
WDLL_EXPORT wint WGetInt(const WObj* obj);

/**
* Get the value from a float object.
*
* @param obj the object to get the value from.
* @return the float value of the object.
*/
WDLL_EXPORT wfloat WGetFloat(const WObj* obj);

/**
* Get the value from a string object.
*
* @param obj the object to get the value from.
* @return the string value of the object. This string is owned by the function and should not be freed.
*/
WDLL_EXPORT const char* WGetString(const WObj* obj);

/**
* Get the value from a function object.
*
* @param obj the object to get the value from.
* @param fn a pointer to a WFunc struct to retrieve the function information.
*/
WDLL_EXPORT void WGetFunction(const WObj* obj, WFuncDesc* fn);

WDLL_EXPORT void* WTryGetUserdata(const WObj* obj, const char* type);

/**
* Get the finalizer of an object.
*
* @param obj the object to get the finalizer from.
* @param finalizer a pointer to a WFinalizer struct to retrieve the finalizer information.
*/
WDLL_EXPORT void WGetFinalizer(const WObj* obj, WFinalizer* finalizer);

/**
* Set the finalizer of an object.
*
* The finalizer is run when the object is garbage collected.
* Do not instantiate any objects in the finalizer.
* 
* @param obj the object to set the finalizer for.
* @param finalizer a pointer to a WFinalizer struct containing the finalizer information.
*/
WDLL_EXPORT void WSetFinalizer(WObj* obj, const WFinalizer* finalizer);

/**
* Get an attribute of an object.
* 
* @param obj the object to get the attribute from.
* @param member a null terminated ASCII string containing the attribute name to get.
* @return the attribute value, or nullptr if the attribute does not exist.
*/
WDLL_EXPORT WObj* WGetAttribute(WObj* obj, const char* member);

/**
* Set an attribute of an object.
* 
* @param obj the object to set the attribute for.
* @param member a null terminated ASCII string containing the attribute name to set.
* @param value the value to set the attribute to.
*/
WDLL_EXPORT void WSetAttribute(WObj* obj, const char* member, WObj* value);

/**
* Get an attribute from the base class of an object.
* 
* Use this function if the attribute is shadowed by the derived class.
* If multiple bases contain this attribute, the first attribute found is returned.
*
* @param obj the object to get the attribute from.
* @param member a null terminated ASCII string containing the attribute name to get.
* @param baseClass the base class to search in, or nullptr to search in all bases.
* @return the attribute value, or nullptr if the attribute does not exist.
*/
WDLL_EXPORT WObj* WGetAttributeFromBase(WObj* obj, const char* member, WObj* baseClass WDEFAULT_ARG(nullptr));

/**
* Iterate over an iterable object.
* 
* This requires the iterable object to implement a __iter__() method
* which returns an iterator. The iterator must implement a __next__() method
* to advance the iterator and a __end__() method to indicate termination.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
* 
* @param obj the object to iterate over.
* @param userdata userdata to be passed to the callback function.
* @param callback a callback function to be called for each value iterated.
*                 This callback should return false to abort iteration
*                 due to an error, and otherwise true.
* @return true on success, or false on failure.
*/
WDLL_EXPORT bool WIterate(WObj* obj, void* userdata, bool(*callback)(WObj* value, void* userdata));

/**
* Check if an object is truthy.
* 
* This requires the object to implement a __nonzero__() method and
* that this method returns a boolean type.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
* 
* @param obj the object to operate on.
* @return a boolean object, or nullptr on failure.
*/
WDLL_EXPORT WObj* WConvertToBool(WObj* obj);

/**
* Convert an object to an integer.
*
* This requires the object to implement a __int__() method and
* that this method returns an integer type.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param obj the object to operate on.
* @return a integer object, or nullptr on failure.
*/
WDLL_EXPORT WObj* WConvertToInt(WObj* obj);

/**
* Convert an object to a float.
*
* This requires the object to implement a __float__() method and
* that this method returns an float type.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param obj the object to operate on.
* @return a float object, or nullptr on failure.
*/
WDLL_EXPORT WObj* WConvertToFloat(WObj* obj);

/**
* Convert an object to a string.
*
* This requires the object to implement a __str__() method and
* that this method returns an string type.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param obj the object to operate on.
* @return a string object, or nullptr on failure.
*/
WDLL_EXPORT WObj* WConvertToString(WObj* obj);

/**
* Call a callable object.
* 
* If the object is a function object then the function is called.
* If the object is a class object then the class is instantiated.
* Otherwise the object's __call__() method is called.
* The callable object and arguments are protected from garbage
* collection while the function call is being executed.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
* 
* @param callable the object to call.
* @param argv a pointer to an array of objects used as arguments to the function call.
*             If argc is 0 then this can be nullptr.
* @param argc the number of arguments to pass.
* @param kwargsDict a dictionary object containing the keyword arguments or nullptr if none.
* @return the return value of the callable, or nullptr on failure.
*/
WDLL_EXPORT WObj* WCall(WObj* callable, WObj** argv, int argc, WObj* kwargsDict WDEFAULT_ARG(nullptr));

/**
* Call a method.
* 
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
* 
* @param obj the object to call the method on.
* @param member a null terminated ASCII string containing the name of the method to call.
* @param argv a pointer to an array of objects used as arguments to the method call.
*             If argc is 0 then this can be nullptr.
* @param argc the number of arguments to pass.
* @param kwargsDict a dictionary object containing the keyword arguments or nullptr if none.
* @return the return value of the method, or nullptr on failure.
*/
WDLL_EXPORT WObj* WCallMethod(WObj* obj, const char* member, WObj** argv, int argc, WObj* kwargsDict WDEFAULT_ARG(nullptr));

/**
* Call a method from the base class.
*
* Use this function if the method is shadowed by the derived class.
* If multiple bases contain this method, the first method found is called.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param obj the object to call the method on.
* @param member a null terminated ASCII string containing the name of the method to call.
* @param argv a pointer to an array of objects used as arguments to the method call.
*             If argc is 0 then this can be nullptr.
* @param argc the number of arguments to pass.
* @param baseClass the base class to search in, or nullptr to search in all bases.
* @return the return value of the method, or nullptr on failure.
*/
WDLL_EXPORT WObj* WCallMethodFromBase(WObj* obj, const char* member, WObj** argv, int argc, WObj* kwargsDict WDEFAULT_ARG(nullptr), WObj* baseClass WDEFAULT_ARG(nullptr));

/**
* Get at an index of an object. i.e. obj[index]
* 
* This requires the object to implement a __getitem__() method.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
* 
* @param obj the object to index.
* @param index the index to get at.
* @return the object at the specified index, or nullptr on failure.
*/
WDLL_EXPORT WObj* WGetIndex(WObj* obj, WObj* index);

/**
* Set at an index of an object i.e. obj[index] = value
* 
* This requires the object to implement a __setitem__() method.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
* 
* @param obj the object to index.
* @param index the index to set at.
* @param value the value to set.
* @return the new object at the specified index, or nullptr on failure.
*/
WDLL_EXPORT WObj* WSetIndex(WObj* obj, WObj* index, WObj* value);

/**
* Apply identity operator i.e. +obj
* 
* This requires the object to implement a __pos__() method.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
* 
* @param obj the object to operate on.
* @return the result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WPositive(WObj* obj);

/**
* Negate an object i.e. -obj
* 
* This requires the object to implement a __neg__() method.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
* 
* @param obj the object to operate on.
* @return the result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WNegative(WObj* obj);

/**
* Add two objects i.e. lhs + rhs
*
* This requires the lhs operand to implement a __add__() method.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param lhs the left hand operand.
* @param rhs the right hand operand.
* @return the result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WAdd(WObj* lhs, WObj* rhs);

/**
* Subtract two objects i.e. lhs - rhs
*
* This requires the lhs operand to implement a __sub__() method.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param lhs the left hand operand.
* @param rhs the right hand operand.
* @return the result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WSubtract(WObj* lhs, WObj* rhs);

/**
* Multiply two objects i.e. lhs * rhs
*
* This requires the lhs operand to implement a __mul__() method.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param lhs the left hand operand.
* @param rhs the right hand operand.
* @return the result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WMultiply(WObj* lhs, WObj* rhs);

/**
* Divide two objects i.e. lhs / rhs
*
* This requires the lhs operand to implement a __truediv__() method.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param lhs the left hand operand.
* @param rhs the right hand operand.
* @return the result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WDivide(WObj* lhs, WObj* rhs);

/**
* Floor divide two objects i.e. lhs // rhs
*
* This requires the lhs operand to implement a __floordiv__() method.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param lhs the left hand operand.
* @param rhs the right hand operand.
* @return the result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WFloorDivide(WObj* lhs, WObj* rhs);

/**
* Modulo two objects i.e. lhs % rhs
*
* This requires the lhs operand to implement a __mod__() method.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param lhs the left hand operand.
* @param rhs the right hand operand.
* @return the result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WModulo(WObj* lhs, WObj* rhs);

/**
* Raise an object to a power i.e. lhs ** rhs
*
* This requires the lhs operand to implement a __pow__() method.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param lhs the left hand operand.
* @param rhs the right hand operand.
* @return the result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WPower(WObj* lhs, WObj* rhs);

/**
* Check equality of two objects i.e. lhs == rhs
*
* This requires the lhs operand to implement a __eq__() method.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param lhs the left hand operand.
* @param rhs the right hand operand.
* @return the result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WEquals(WObj* lhs, WObj* rhs);

/**
* Check inequality of two objects i.e. lhs != rhs
*
* This requires the lhs operand to implement a __ne__() method and
* that this method returns a boolean type.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param lhs the left hand operand.
* @param rhs the right hand operand.
* @return the result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WNotEquals(WObj* lhs, WObj* rhs);

/**
* Compare less than two objects i.e. lhs < rhs
*
* This requires the lhs operand to implement a __lt__() method and
* that this method returns a boolean type.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param lhs the left hand operand.
* @param rhs the right hand operand.
* @return the result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WLessThan(WObj* lhs, WObj* rhs);

/**
* Compare less than or equal two objects i.e. lhs <= rhs
*
* This requires the lhs operand to implement a __le__() method and
* that this method returns a boolean type.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param lhs the left hand operand.
* @param rhs the right hand operand.
* @return the result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WLessThanOrEqual(WObj* lhs, WObj* rhs);

/**
* Compare greater than two objects i.e. lhs > rhs
*
* This requires the lhs operand to implement a __gt__() method and
* that this method returns a boolean type.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param lhs the left hand operand.
* @param rhs the right hand operand.
* @return the result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WGreaterThan(WObj* lhs, WObj* rhs);

/**
* Compare greater than or equal two objects i.e. lhs >= rhs
*
* This requires the lhs operand to implement a __ge__() method and
* that this method returns a boolean type.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param lhs the left hand operand.
* @param rhs the right hand operand.
* @return the result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WGreaterThanOrEqual(WObj* lhs, WObj* rhs);

/**
* Check if an object is contained within another object i.e. obj in container
*
* This requires the container to implement a __contains__() method and
* that this method returns a boolean type.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param container the container.
* @param obj the value to check.
* @return the result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WIn(WObj* container, WObj* obj);

/**
* Check if an object is not contained within another object i.e. obj not in container
*
* This requires the container to implement a __contains__() method and
* that this method returns a boolean type.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param container the container.
* @param obj the value to check.
* @return the result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WNotIn(WObj* container, WObj* obj);

/**
* Bitwise-and two objects i.e. lhs & rhs
*
* This requires the lhs operand to implement a __and__() method.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param lhs the left hand operand.
* @param rhs the right hand operand.
* @return the result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WBitAnd(WObj* lhs, WObj* rhs);

/**
* Bitwise-or two objects i.e. lhs | rhs
*
* This requires the lhs operand to implement a __or__() method.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param lhs the left hand operand.
* @param rhs the right hand operand.
* @return the result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WBitOr(WObj* lhs, WObj* rhs);

/**
* Bitwise-complement an object i.e. ~obj
*
* This requires the object to implement a __invert__() method.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param obj the object to operate on.
* @return the result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WBitNot(WObj* obj);

/**
* Bitwise-xor two objects i.e. lhs ^ rhs
*
* This requires the lhs operand to implement a __xor__() method.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param lhs the left hand operand.
* @param rhs the right hand operand.
* @return the result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WBitXor(WObj* lhs, WObj* rhs);

/**
* Bitshift left two objects i.e. lhs << rhs
*
* This requires the lhs operand to implement a __lshift__() method.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param lhs the left hand operand.
* @param rhs the right hand operand.
* @return the result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WShiftLeft(WObj* lhs, WObj* rhs);

/**
* Bitshift right two objects i.e. lhs >> rhs
*
* This requires the lhs operand to implement a __rshift__() method.
* Call WGetErrorCode() or WGetErrorMessage() to get any errors.
*
* @param lhs the left hand operand.
* @param rhs the right hand operand.
* @return the result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WShiftRight(WObj* lhs, WObj* rhs);

#undef WDEFAULT_ARG
#undef WDLL_EXPORT

#ifdef __cplusplus
} // extern "C"
#endif
