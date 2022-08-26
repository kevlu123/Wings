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

typedef WObj* (*WFunction)(WObj** args, int argc, WObj* kwargs, void* userdata);
typedef void (*WFinalizer)(WObj* obj, void* userdata);
typedef void (*WPrintFunction)(const char* message, int len, void* userdata);
typedef void (*WErrorCallback)(const char* message, void* userdata);
typedef bool (*WIterationCallback)(WObj* obj, void* userdata);

struct WFuncDesc {
    WFunction fptr;
    void* userdata;
    bool isMethod;
    const char* tag;
    const char* prettyName;
};

struct WFinalizerDesc {
    WFinalizer fptr;
    void* userdata;
};

struct WConfig {
    int maxAlloc;
    int maxRecursion;
    int maxCollectionSize;
    float gcRunFactor;
    WPrintFunction print;
    void* printUserdata;
};

#if defined(_WIN32) && defined(_WINDLL)
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
* Remarks:
* This function always succeeds.
* The returned context must be freed with WDestroyContext().
* 
* @param config A pointer to a WConfig struct containing configuration information,
*               or nullptr for the default configuration.
* @return A newly created context.
*/
WDLL_EXPORT WContext* WCreateContext(const WConfig* config WDEFAULT_ARG(nullptr));

/**
* Free a context created with WCreateContext().
* 
* @param context The context to free.
*/
WDLL_EXPORT void WDestroyContext(WContext* context);

/**
* Compile a script to a function object.
* 
* Remarks:
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
* 
* @param context The relevant context.
* @param code A null terminated ASCII string containing the source code.
* @param tag An optional null terminated ASCII string to be displayed
*            in error messages relating to this script. This parameter may be nullptr.
* @return A function object, or nullptr on failure. Call WCall() to execute the function object.
*/
WDLL_EXPORT WObj* WCompile(WContext* context, const char* code, const char* tag WDEFAULT_ARG(nullptr));

/**
* Set a function to be called when a programmer error occurs.
*
* @param callback The function to call or nullptr to use abort() instead.
* @param userdata User data to pass to the callback.
*/
WDLL_EXPORT void WSetErrorCallback(WErrorCallback callback, void* userdata);

/**
* Get the current error string.
* 
* @param context The relevant context.
* @return The current error as a null terminated ASCII string.
*         This string is owned by the function and should not be freed.
*/
WDLL_EXPORT const char* WGetErrorMessage(WContext* context);

/**
* Get the current exception object.
*
* @param context The relevant context.
* @return The current exception object, or nullptr if there is no exception.
*/
WDLL_EXPORT WObj* WGetCurrentException(WContext* context);

/**
* Create and raise an exception.
* 
* Remarks:
* If an exception is already set, the old exception will be overwritten.
* 
* @param context The relevant context.
* @param message A null terminated ASCII string containing the error message string,
*                or nullptr for an empty string.
* @param type A class object of the type of exception to be raised,
*             or nullptr for Exception. The type must be a subclass of BaseException.
*/
WDLL_EXPORT void WRaiseException(WContext* context, const char* message WDEFAULT_ARG(nullptr), WObj* type WDEFAULT_ARG(nullptr));

/**
* Raise an existing exception object.
*
* Remarks:
* If an exception is already set, the old exception will be overwritten.
*
* @param context The relevant context.
* @param type The exception object to raise. The type must be a subclass of BaseException.
*/
WDLL_EXPORT void WRaiseExceptionObject(WContext* context, WObj* exception);

/**
* Raise a TypeError with a formatted message.
*
* Remarks:
* If an exception is already set, the old exception will be overwritten.
*
* @param context The relevant context.
* @param given The number of arguments given.
* @param expected The number of arguments expected, or -1 if this is not a fixed number.
*/
WDLL_EXPORT void WRaiseArgumentCountError(WContext* context, int given, int expected);

/**
* Raise a TypeError with a formatted message.
*
* Remarks:
* If an exception is already set, the old exception will be overwritten.
*
* @param context The relevant context.
* @param argIndex The parameter index of the invalid argument.
* @param expected A null terminated ASCII string describing the expected type.
*/
WDLL_EXPORT void WRaiseArgumentTypeError(WContext* context, int argIndex, const char* expected);

/**
* Raise a AttributeError with a formatted message.
*
* Remarks:
* If an exception is already set, the old exception will be overwritten.
*
* @param obj The object that does not have the attribute.
* @param attribute A null terminated ASCII string containing the attribute.
*/
WDLL_EXPORT void WRaiseAttributeError(const WObj* obj, const char* attribute);

/**
* Raise a ZeroDivisionError.
*
* Remarks:
* If an exception is already set, the old exception will be overwritten.
*
* @param context The relevant context.
*/
WDLL_EXPORT void WRaiseZeroDivisionError(WContext* context);

/**
* Raise a IndexError.
*
* Remarks:
* If an exception is already set, the old exception will be overwritten.
*
* @param context The relevant context.
*/
WDLL_EXPORT void WRaiseIndexError(WContext* context);

/**
* Raise a StopIteration to terminate iteration.
*
* Remarks:
* If an exception is already set, the old exception will be overwritten.
*
* @param context The relevant context.
*/
WDLL_EXPORT void WRaiseStopIteration(WContext* context);

/**
* Raise a TypeError.
*
* Remarks:
* If an exception is already set, the old exception will be overwritten.
*
* @param context The relevant context.
* @param message A null terminated ASCII string containing the exception message,
*                or nullptr for an empty string.
*/
WDLL_EXPORT void WRaiseTypeError(WContext* context, const char* message WDEFAULT_ARG(nullptr));

/**
* Raise a ValueError.
*
* Remarks:
* If an exception is already set, the old exception will be overwritten.
*
* @param context The relevant context.
* @param message A null terminated ASCII string containing the exception message,
*                or nullptr for an empty string.
*/
WDLL_EXPORT void WRaiseValueError(WContext* context, const char* message WDEFAULT_ARG(nullptr));

/**
* Raise a NameError.
*
* Remarks:
* If an exception is already set, the old exception will be overwritten.
*
* @param context The relevant context.
* @param name A null terminated ASCII string containing the name that was not found.
*/
WDLL_EXPORT void WRaiseNameError(WContext* context, const char* name);

/**
* Check if an object's class derives from any of the specified classes.
* 
* @param instance The object to be checked.
* @param types An array of class objects to be checked against.
*              This can be nullptr if typesLen is 0.
* @param typesLen The length of the types array.
* @return The first subclass matched, or nullptr if the object's
*         class does not derive from any of the specified classes.
*/
WDLL_EXPORT WObj* WIsInstance(const WObj* instance, WObj*const* types, int typesLen);

/**
* Clear the current exception.
*
* @param context The relevant context.
*/
WDLL_EXPORT void WClearCurrentException(WContext* context);

/**
* Get the current configuration.
* 
* @param context The relevant context.
* @param out A pointer to a WConfig struct to receive the configuration information.
*/
WDLL_EXPORT void WGetConfig(const WContext* context, WConfig* out);

/**
* Set the current configuration.
* 
* @param context The relevant context.
* @param config The configuration information.
*/
WDLL_EXPORT void WSetConfig(WContext* context, const WConfig* config);

/**
* Get a global variable.
* 
* @param context The relevant context.
* @param name A null terminated ASCII string containing the name of the
*             global variable to retrieve.
* @return The value of the global variable, or nullptr if it does not exist.
*/
WDLL_EXPORT WObj* WGetGlobal(WContext* context, const char* name);

/**
* Set a global variable.
* 
* @param context The relevant context.
* @param name A null terminated ASCII string containing the name of the
*        global variable to set.
* @param value The value to set the global variable to.
*/
WDLL_EXPORT void WSetGlobal(WContext* context, const char* name, WObj* value);

/**
* Delete a global variable.
*
* Remarks:
* This function only unbinds a value from a global name
* and does not free the associated value nor call __del__().
* If the previously bound value has no references, WCollectGarbage()
* can be used to destroy the object and free the memory.
* 
* @param context The relevant context.
* @param name A null terminated ASCII string containing the name of the
*        global variable to set.
*/
WDLL_EXPORT void WDeleteGlobal(WContext* context, const char* name);

/**
* Print a message.
* 
* Remarks:
* This function uses the print function specified in the current configuration.
* By default this is std::cout.
* 
* @param context The relevant context.
* @param message A pointer to a byte array containing the message. This can be
*                nullptr if len is 0.
* @param len The length of the message byte array.
*/
WDLL_EXPORT void WPrint(const WContext* context, const char* message, int len);

/**
* Print a message.
*
* Remarks:
* This function uses the print function specified in the current configuration.
* By default this is std::cout.
*
* @param context The relevant context.
* @param message A pointer to a null terminated byte array containing the message.
*/
WDLL_EXPORT void WPrintString(const WContext* context, const char* message);

/**
* Run the garbage collector and free all unreachable objects.
* 
* @param context The relevant context.
*/
WDLL_EXPORT void WCollectGarbage(WContext* context);

/**
* Protect an object from being garbage collected.
* 
* Remarks:
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
* Remarks:
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
* Get the None singleton value.
* 
* Remarks:
* Unlike the other WCreateXXX() functions, this function always succeeds
* since None is a singleton value which is already allocated.
* 
* @param context The relevant context.
* @return The None singleton value.
*/
WDLL_EXPORT WObj* WCreateNone(WContext* context);

/**
* Instantiate a boolean object.
* 
* Remarks:
* Unlike the other WCreateXXX() functions, this function always succeeds
* due to interning.
* 
* @param context The relevant context.
* @param value The value of the object.
* @return The instantiated object.
*/
WDLL_EXPORT WObj* WCreateBool(WContext* context, bool value WDEFAULT_ARG(false));

/**
* Instantiate an integer object.
*
* Remarks:
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param context The relevant context.
* @param value The value of the object.
* @return The instantiated object, or nullptr on failure.
*/
WDLL_EXPORT WObj* WCreateInt(WContext* context, wint value WDEFAULT_ARG(0));

/**
* Instantiate a float object.
*
* Remarks:
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param context The relevant context.
* @param value The value of the object.
* @return The instantiated object, or nullptr on failure.
*/
WDLL_EXPORT WObj* WCreateFloat(WContext* context, wfloat value WDEFAULT_ARG(0));

/**
* Instantiate a string object.
*
* Remarks:
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param context The relevant context.
* @param value A null terminated ASCII string, or nullptr for an empty string.
* @return The instantiated object, or nullptr on failure.
*/
WDLL_EXPORT WObj* WCreateString(WContext* context, const char* value WDEFAULT_ARG(nullptr));

/**
* Instantiate a tuple object.
*
* Remarks:
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param context The relevant context.
* @param argv A pointer to an array of objects to initialise the tuple with.
*             This can be nullptr if argc is 0.
* @param argc The number of objects to initialise the tuple with.
* @return The instantiated object, or nullptr on failure.
*/
WDLL_EXPORT WObj* WCreateTuple(WContext* context, WObj** argv, int argc);

/**
* Instantiate a list object.
*
* Remarks:
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param context The relevant context.
* @param argv A pointer to an array of objects to initialise the list with.
*             This can be nullptr if argc is 0.
* @param argc The number of objects to initialise the list with.
* @return The instantiated object, or nullptr on failure.
*/
WDLL_EXPORT WObj* WCreateList(WContext* context, WObj** argv WDEFAULT_ARG(nullptr), int argc WDEFAULT_ARG(0));

/**
* Instantiate a dictionary object.
*
* Remarks:
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param context The relevant context.
* @param keys A pointer to an array of keys to initialise the dictionary with.
*             This can be nullptr if argc is 0.
* @param values A pointer to an array of values to initialise the dictionary with.
*             This can be nullptr if argc is 0.
* @param argc The number of key value pairs to initialise the dictionary with.
* @return The instantiated object, or nullptr on failure.
*/
WDLL_EXPORT WObj* WCreateDictionary(WContext* context, WObj** keys WDEFAULT_ARG(nullptr), WObj** values WDEFAULT_ARG(nullptr), int argc WDEFAULT_ARG(0));

/**
* Instantiate a function object.
*
* Remarks:
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param context The relevant context.
* @param value A function description.
* @return The instantiated object, or nullptr on failure.
*/
WDLL_EXPORT WObj* WCreateFunction(WContext* context, const WFuncDesc* value);

/**
* Instantiate a class object.
*
* Remarks:
* If no bases are specified, object is implicitly used as a base.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param context The relevant context.
* @param name A null terminated ASCII string containing the name of the class.
*             Do not use a name beginning with two underscores.
* @param bases A pointer to an array of class objects to be used as a base.
*              This can be nullptr if baseCount is 0.
* @param baseCount The length of the bases array.
* @return The instantiated object, or nullptr on failure.
*/
WDLL_EXPORT WObj* WCreateClass(WContext* context, const char* name, WObj** bases, int baseCount);

/**
* Add an attribute to a class.
* 
* Remarks:
* Existing instances of the class will not gain the attribute.
* 
* @param class_ The class to add the attribute to.
* @param attribute A null terminated ASCII string containing the attribute name.
* @param value The value of the attribute.
*/
WDLL_EXPORT void WAddAttributeToClass(WObj* class_, const char* attribute, WObj* value);

/**
* Check if an object is None.
* 
* @param obj The object to inspect.
* @return True if the object is None, otherwise false.
*/
WDLL_EXPORT bool WIsNone(const WObj* obj);

/**
* Check if an object is a boolean.
* 
* @param obj The object to inspect.
* @return True if the object is a boolean, otherwise false.
*/
WDLL_EXPORT bool WIsBool(const WObj* obj);

/**
* Check if an object is an integer.
* 
* @param obj The object to inspect.
* @return True if the object is an integer, otherwise false.
*/
WDLL_EXPORT bool WIsInt(const WObj* obj);

/**
* Check if an object is a float.
* 
* @param obj The object to inspect.
* @return True if the object is an integer or float, otherwise false.
*/
WDLL_EXPORT bool WIsIntOrFloat(const WObj* obj);

/**
* Check if an object is a string.
* 
* @param obj The object to inspect.
* @return True if the object is a string, otherwise false.
*/
WDLL_EXPORT bool WIsString(const WObj* obj);

/**
* Check if an object is a tuple.
* 
* @param obj The object to inspect.
* @return True if the object is a tuple, otherwise false.
*/
WDLL_EXPORT bool WIsTuple(const WObj* obj);

/**
* Check if an object is a list.
* 
* @param obj The object to inspect.
* @return True if the object is a list, otherwise false.
*/
WDLL_EXPORT bool WIsList(const WObj* obj);

/**
* Check if an object is a dictionary.
* 
* @param obj The object to inspect.
* @return True if the object is a dictionary, otherwise false.
*/
WDLL_EXPORT bool WIsDictionary(const WObj* obj);

/**
* Check if an object is a function.
* 
* @param obj The object to inspect.
* @return True if the object is a function, otherwise false.
*/
WDLL_EXPORT bool WIsFunction(const WObj* obj);

/**
* Check if an object is a class.
* 
* @param obj The object to inspect.
* @return True if the object is a class, otherwise false.
*/
WDLL_EXPORT bool WIsClass(const WObj* obj);

/**
* Check if an object is an immutable type.
*
* Remarks:
* Immutable types are None, bool, int, float, str, and tuples with immutable elements.
* 
* @param obj The object to inspect.
* @return True if the object is an immutable type, otherwise false.
*/
WDLL_EXPORT bool WIsImmutableType(const WObj* obj);

/**
* Get the value from a boolean object.
* 
* @param obj The object to get the value from.
* @return The boolean value of the object.
*/
WDLL_EXPORT bool WGetBool(const WObj* obj);

/**
* Get the value from an integer object.
*
* @param obj The object to get the value from.
* @return The integer value of the object.
*/
WDLL_EXPORT wint WGetInt(const WObj* obj);

/**
* Get the float value from an integer or float object.
*
* @param obj The object to get the value from.
* @return The float value of the object.
*/
WDLL_EXPORT wfloat WGetFloat(const WObj* obj);

/**
* Get the value from a string object.
*
* @param obj The object to get the value from.
* @return The string value of the object. This string is
*         owned by the function and should not be freed.
*/
WDLL_EXPORT const char* WGetString(const WObj* obj);

/**
* Set the userdata for an object.
* 
* @param obj The object to set the userdata for.
* @param userdata The userdata to set.
*/
WDLL_EXPORT void WSetUserdata(WObj* obj, void* userdata);

/**
* Try to get the userdata from an object.
*
* @param obj The object to get the value from.
* @param type A null terminated ASCII string containing the name of
*             the type to query.
* @param out A pointer to a void* to receive the userdata.
* @return True if obj matches the type given, otherwise false.
*/
WDLL_EXPORT bool WTryGetUserdata(const WObj* obj, const char* type, void** out);

/**
* Get the finalizer of an object.
*
* @param obj The object to get the finalizer from.
* @param finalizer A finalizer description.
*/
WDLL_EXPORT void WGetFinalizer(const WObj* obj, WFinalizerDesc* finalizer);

/**
* Set the finalizer of an object.
*
* The finalizer is run when the object is garbage collected.
* Do not instantiate any objects in the finalizer.
* 
* @param obj The object to set the finalizer for.
* @param finalizer The received finalizer description.
*/
WDLL_EXPORT void WSetFinalizer(WObj* obj, const WFinalizerDesc* finalizer);

/**
* Check if an object contains an attribute.
*
* @param obj The object to get the attribute from.
* @param member A null terminated ASCII string containing the attribute name to get.
* @return The attribute value, or nullptr if the attribute does not exist.
*/
WDLL_EXPORT WObj* WHasAttribute(WObj* obj, const char* member);

/**
* Get an attribute of an object.
* If the attribute does not exist, an AttributeError is raised.
* 
* @param obj The object to get the attribute from.
* @param member A null terminated ASCII string containing the attribute name to get.
* @return The attribute value, or nullptr if the attribute does not exist.
*/
WDLL_EXPORT WObj* WGetAttribute(WObj* obj, const char* member);

/**
* Set an attribute of an object.
* 
* @param obj The object to set the attribute for.
* @param member A null terminated ASCII string containing the attribute name to set.
* @param value The value to set the attribute to.
*/
WDLL_EXPORT void WSetAttribute(WObj* obj, const char* member, WObj* value);

/**
* Get an attribute from the base class of an object.
* 
* Remarks:
* Use this function if the attribute is shadowed by the derived class.
* If multiple bases contain this attribute, the first attribute found is returned.
*
* @param obj The object to get the attribute from.
* @param member A null terminated ASCII string containing the attribute name to get.
* @param baseClass The base class to search in, or nullptr to search in all bases.
* @return The attribute value, or nullptr if the attribute does not exist.
*/
WDLL_EXPORT WObj* WGetAttributeFromBase(WObj* obj, const char* member, WObj* baseClass WDEFAULT_ARG(nullptr));

/**
* Iterate over an iterable object.
* 
* Remarks:
* This requires the iterable object to implement a __iter__() method
* which returns an iterator. The iterator must implement a __next__() method
* to advance the iterator and yield a value. When the iterator is exhausted,
* it should raise a StopIteration exception.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
* 
* @param obj The object to iterate over.
* @param userdata The userdata to be passed to the callback function.
* @param callback A callback function to be called for each value iterated.
*                 This callback should return true on a successful iteration,
*                 otherwise it should return false to signal an error and abort iteration.
* @return true on success, or false on failure.
*/
WDLL_EXPORT bool WIterate(WObj* obj, void* userdata, WIterationCallback callback);

/**
* Check if an object is truthy.
* 
* Remarks:
* This requires the object to implement a __nonzero__() method and
* that this method returns a boolean type.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
* 
* @param obj The object to operate on.
* @return A boolean object, or nullptr on failure.
*/
WDLL_EXPORT WObj* WConvertToBool(WObj* obj);

/**
* Convert an object to an integer.
*
* Remarks:
* This requires the object to implement a __int__() method and
* that this method returns an integer type.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param obj The object to operate on.
* @return An integer object, or nullptr on failure.
*/
WDLL_EXPORT WObj* WConvertToInt(WObj* obj);

/**
* Convert an object to a float.
*
* Remarks:
* This requires the object to implement a __float__() method and
* that this method returns an float type.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param obj The object to operate on.
* @return A float object, or nullptr on failure.
*/
WDLL_EXPORT WObj* WConvertToFloat(WObj* obj);

/**
* Convert an object to a string.
*
* Remarks:
* This requires the object to implement a __str__() method and
* that this method returns an string type.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param obj The object to operate on.
* @return A string object, or nullptr on failure.
*/
WDLL_EXPORT WObj* WConvertToString(WObj* obj);

/**
* Call a callable object.
* 
* Remarks:
* If the object is a function object then the function is called.
* If the object is a class object then the class is instantiated.
* Otherwise the object's __call__() method is called.
* The callable object and arguments are protected from garbage
* collection while the function call is being executed.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
* 
* @param callable The object to call.
* @param argv A pointer to an array of objects used as arguments to the function call.
*             If argc is 0 then this can be nullptr.
* @param argc The number of arguments to pass.
* @param kwargsDict A dictionary object containing the keyword arguments or nullptr if none.
* @return The return value of the callable, or nullptr on failure.
*/
WDLL_EXPORT WObj* WCall(WObj* callable, WObj** argv, int argc, WObj* kwargsDict WDEFAULT_ARG(nullptr));

/**
* Call a method.
* 
* Remarks:
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
* 
* @param obj The object to call the method on.
* @param member A null terminated ASCII string containing the name of the method to call.
* @param argv A pointer to an array of objects used as arguments to the method call.
*             If argc is 0 then this can be nullptr.
* @param argc The number of arguments to pass.
* @param kwargsDict A dictionary object containing the keyword arguments or nullptr if none.
* @return The return value of the method, or nullptr on failure.
*/
WDLL_EXPORT WObj* WCallMethod(WObj* obj, const char* member, WObj** argv, int argc, WObj* kwargsDict WDEFAULT_ARG(nullptr));

/**
* Call a method from the base class.
*
* Remarks:
* Use this function if the method is shadowed by the derived class.
* If multiple bases contain this method, the first method found is called.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param obj The object to call the method on.
* @param member A null terminated ASCII string containing the name of the method to call.
* @param argv A pointer to an array of objects used as arguments to the method call.
*             If argc is 0 then this can be nullptr.
* @param argc The number of arguments to pass.
* @param baseClass The base class to search in, or nullptr to search in all bases.
* @return The return value of the method, or nullptr on failure.
*/
WDLL_EXPORT WObj* WCallMethodFromBase(WObj* obj, const char* member, WObj** argv, int argc, WObj* kwargsDict WDEFAULT_ARG(nullptr), WObj* baseClass WDEFAULT_ARG(nullptr));

/**
* Get at an index of an object. i.e. obj[index]
* 
* Remarks:
* This requires the object to implement a __getitem__() method.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
* 
* @param obj The object to index.
* @param index The index to get at.
* @return The object at the specified index, or nullptr on failure.
*/
WDLL_EXPORT WObj* WGetIndex(WObj* obj, WObj* index);

/**
* Set at an index of an object i.e. obj[index] = value
* 
* This requires the object to implement a __setitem__() method.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
* 
* @param obj The object to index.
* @param index The index to set at.
* @param value The value to set.
* @return The new object at the specified index, or nullptr on failure.
*/
WDLL_EXPORT WObj* WSetIndex(WObj* obj, WObj* index, WObj* value);

/**
* Apply identity operator i.e. +obj
* 
* Remarks:
* This requires the object to implement a __pos__() method.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
* 
* @param obj The object to operate on.
* @return The result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WPositive(WObj* obj);

/**
* Negate an object i.e. -obj
* 
* Remarks:
* This requires the object to implement a __neg__() method.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
* 
* @param obj The object to operate on.
* @return The result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WNegative(WObj* obj);

/**
* Add two objects i.e. lhs + rhs
*
* Remarks:
* This requires the lhs operand to implement a __add__() method.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param lhs The left hand side operand.
* @param rhs The right side hand side operand.
* @return The result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WAdd(WObj* lhs, WObj* rhs);

/**
* Subtract two objects i.e. lhs - rhs
*
* Remarks:
* This requires the lhs operand to implement a __sub__() method.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param lhs The left hand side operand.
* @param rhs The right side hand side operand.
* @return The result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WSubtract(WObj* lhs, WObj* rhs);

/**
* Multiply two objects i.e. lhs * rhs
*
* Remarks:
* This requires the lhs operand to implement a __mul__() method.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param lhs The left hand side operand.
* @param rhs The right side hand side operand.
* @return The result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WMultiply(WObj* lhs, WObj* rhs);

/**
* Divide two objects i.e. lhs / rhs
*
* Remarks:
* This requires the lhs operand to implement a __truediv__() method.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param lhs The left hand side operand.
* @param rhs The right side hand side operand.
* @return The result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WDivide(WObj* lhs, WObj* rhs);

/**
* Floor divide two objects i.e. lhs // rhs
*
* Remarks:
* This requires the lhs operand to implement a __floordiv__() method.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param lhs The left hand side operand.
* @param rhs The right side hand side operand.
* @return The result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WFloorDivide(WObj* lhs, WObj* rhs);

/**
* Take the modulus of two objects i.e. lhs % rhs
*
* Remarks:
* This requires the lhs operand to implement a __mod__() method.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param lhs The left hand side operand.
* @param rhs The right side hand side operand.
* @return The result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WModulo(WObj* lhs, WObj* rhs);

/**
* Raise an object to a power i.e. lhs ** rhs
*
* Remarks:
* This requires the lhs operand to implement a __pow__() method.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param lhs The left hand side operand.
* @param rhs The right side hand side operand.
* @return The result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WPower(WObj* lhs, WObj* rhs);

/**
* Check equality of two objects i.e. lhs == rhs
*
* Remarks:
* This requires the lhs operand to implement a __eq__() method.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param lhs The left hand side operand.
* @param rhs The right side hand side operand.
* @return The result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WEquals(WObj* lhs, WObj* rhs);

/**
* Check inequality of two objects i.e. lhs != rhs
*
* Remarks:
* This requires the lhs operand to implement a __ne__() method and
* that this method returns a boolean type.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param lhs The left hand side operand.
* @param rhs The right side hand side operand.
* @return The result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WNotEquals(WObj* lhs, WObj* rhs);

/**
* Compare less than two objects i.e. lhs < rhs
*
* Remarks:
* This requires the lhs operand to implement a __lt__() method and
* that this method returns a boolean type.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param lhs The left hand side operand.
* @param rhs The right side hand side operand.
* @return The result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WLessThan(WObj* lhs, WObj* rhs);

/**
* Compare less than or equal two objects i.e. lhs <= rhs
*
* Remarks:
* This requires the lhs operand to implement a __le__() method and
* that this method returns a boolean type.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param lhs The left hand side operand.
* @param rhs The right side hand side operand.
* @return The result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WLessThanOrEqual(WObj* lhs, WObj* rhs);

/**
* Compare greater than two objects i.e. lhs > rhs
*
* Remarks:
* This requires the lhs operand to implement a __gt__() method and
* that this method returns a boolean type.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param lhs The left hand side operand.
* @param rhs The right side hand side operand.
* @return The result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WGreaterThan(WObj* lhs, WObj* rhs);

/**
* Compare greater than or equal two objects i.e. lhs >= rhs
*
* Remarks:
* This requires the lhs operand to implement a __ge__() method and
* that this method returns a boolean type.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param lhs The left hand side operand.
* @param rhs The right side hand side operand.
* @return The result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WGreaterThanOrEqual(WObj* lhs, WObj* rhs);

/**
* Check if an object is contained within another object i.e. obj in container
*
* Remarks:
* This requires the container to implement a __len__() method and
* that this method returns an integer type.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param obj The object to get the length of.
* @return The result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WLen(WObj* obj);

/**
* Check if an object is contained within another object i.e. obj in container
*
* Remarks:
* This requires the container to implement a __contains__() method and
* that this method returns a boolean type.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param container The container to search in.
* @param obj The value to find.
* @return The result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WIn(WObj* container, WObj* obj);

/**
* Check if an object is not contained within another object i.e. obj not in container
*
* Remarks:
* This requires the container to implement a __contains__() method and
* that this method returns a boolean type.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param container The container to search in.
* @param obj The value to find.
* @return The result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WNotIn(WObj* container, WObj* obj);

/**
* Bitwise-and two objects i.e. lhs & rhs
*
* Remarks:
* This requires the lhs operand to implement a __and__() method.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param lhs The left hand side operand.
* @param rhs The right side hand side operand.
* @return The result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WBitAnd(WObj* lhs, WObj* rhs);

/**
* Bitwise-or two objects i.e. lhs | rhs
*
* Remarks:
* This requires the lhs operand to implement a __or__() method.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param lhs The left hand side operand.
* @param rhs The right side hand side operand.
* @return The result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WBitOr(WObj* lhs, WObj* rhs);

/**
* Bitwise-complement an object i.e. ~obj
*
* Remarks:
* This requires the object to implement a __invert__() method.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param obj The object to operate on.
* @return The result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WBitNot(WObj* obj);

/**
* Bitwise-xor two objects i.e. lhs ^ rhs
*
* Remarks:
* This requires the lhs operand to implement a __xor__() method.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param lhs The left hand side operand.
* @param rhs The right side hand side operand.
* @return The result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WBitXor(WObj* lhs, WObj* rhs);

/**
* Bitshift left two objects i.e. lhs << rhs
*
* Remarks:
* This requires the lhs operand to implement a __lshift__() method.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param lhs The left hand side operand.
* @param rhs The right side hand side operand.
* @return The result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WShiftLeft(WObj* lhs, WObj* rhs);

/**
* Bitshift right two objects i.e. lhs >> rhs
*
* Remarks:
* This requires the lhs operand to implement a __rshift__() method.
* Call WGetCurrentException() or WGetErrorMessage() to get error information.
*
* @param lhs The left hand side operand.
* @param rhs The right side hand side operand.
* @return The result of the operation, or nullptr on failure.
*/
WDLL_EXPORT WObj* WShiftRight(WObj* lhs, WObj* rhs);

#undef WDEFAULT_ARG
#undef WDLL_EXPORT

#ifdef __cplusplus
} // extern "C"
#endif
