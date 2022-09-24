#pragma once
#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif

struct Wg_Context;
struct Wg_Obj;
typedef int64_t Wg_int;
typedef double Wg_float;

typedef Wg_Obj* (*Wg_Function)(Wg_Obj** args, int argc, void* userdata);
typedef void (*Wg_Finalizer)(Wg_Obj* obj, void* userdata);
typedef void (*Wg_PrintFunction)(const char* message, int len, void* userdata);
typedef void (*Wg_ErrorCallback)(const char* message, void* userdata);
typedef bool (*Wg_IterationCallback)(Wg_Obj* obj, void* userdata);

struct Wg_FuncDesc {
	Wg_Function fptr;
	void* userdata;
	bool isMethod;
	const char* tag;
	const char* prettyName;
};

struct Wg_FinalizerDesc {
	Wg_Finalizer fptr;
	void* userdata;
};

struct Wg_Config {
	int maxAlloc;
	int maxRecursion;
	int maxCollectionSize;
	float gcRunFactor;
	Wg_PrintFunction print;
	void* printUserdata;
};

enum Wg_UnOp {
	WG_UOP_POS,			// __pos__
	WG_UOP_NEG,			// __neg__
	WG_UOP_BITNOT,		// __invert__
	WG_UOP_NOT,			// not __nonzero__								(must return bool)
	WG_UOP_HASH,		// __hash__										(must return int)
	WG_UOP_LEN,			// __len__										(must return int)
	WG_UOP_BOOL,		// __nonzero__									(must return bool)
	WG_UOP_INT,			// __int__										(must return int)
	WG_UOP_FLOAT,		// __float__									(must return float)
	WG_UOP_STR,			// __str__										(must return string)
	WG_UOP_REPR,		// __repr__										(must return string)
};

enum Wg_BinOp {
	WG_BOP_ADD,			// __add__
	WG_BOP_SUB,			// __sub__
	WG_BOP_MUL,			// __mul__
	WG_BOP_DIV,			// __truediv__
	WG_BOP_FLOORDIV,	// __floordiv__
	WG_BOP_MOD,			// __mod__
	WG_BOP_POW,			// __pow__
	WG_BOP_BITAND,		// __and__
	WG_BOP_BITOR,		// __or__
	WG_BOP_BITXOR,		// __xor__
	WG_BOP_AND,			// lhs.__nonzero__() and rhs.__nonzero__()		(must return bool)
	WG_BOP_OR,			// lhs.__nonzero__() or rhs.__nonzero__()		(must return bool)
	WG_BOP_SHL,			// __lshift__
	WG_BOP_SHR,			// __rshift__
	WG_BOP_IN,			// __contains__									(must return bool)
	WG_BOP_NOTIN,		// not __contains__								(must return bool)
	WG_BOP_EQ,			// __eq__										(must return bool)
	WG_BOP_NE,			// __ne__										(must return bool)
	WG_BOP_LT,			// __lt__										(must return bool)
	WG_BOP_LE,			// __le__										(must return bool)
	WG_BOP_GT,			// __gt__										(must return bool)
	WG_BOP_GE,			// __ge__										(must return bool)
};

#if defined(_WIN32) && defined(_WINDLL)
#define WG_DLL_EXPORT __declspec(dllexport)
#else
#define WG_DLL_EXPORT
#endif

#ifdef __cplusplus
#define WG_DEFAULT_ARG(arg) = arg
#else
#define WG_DEFAULT_ARG(arg)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
* Create a context (an instance of an interpreter).
* 
* Remarks:
* This function always succeeds.
* The returned context must be freed with Wg_DestroyContext().
* 
* @param config A pointer to a Wg_Config struct containing configuration information,
*               or nullptr for the default configuration.
* @return A newly created context.
*/
WG_DLL_EXPORT Wg_Context* Wg_CreateContext(const Wg_Config* config WG_DEFAULT_ARG(nullptr));

/**
* Free a context created with Wg_CreateContext().
* 
* @param context The context to free.
*/
WG_DLL_EXPORT void Wg_DestroyContext(Wg_Context* context);

/**
* Compile a script to a function object.
* 
* Remarks:
* Call Wg_GetCurrentException() or Wg_GetErrorMessage() to get error information.
* 
* @param context The relevant context.
* @param code A null terminated ASCII string containing the source code.
* @param tag An optional null terminated ASCII string to be displayed
*            in error messages relating to this script. This parameter may be nullptr.
* @return A function object, or nullptr on failure. Call Wg_Call() to execute the function object.
*/
WG_DLL_EXPORT Wg_Obj* Wg_Compile(Wg_Context* context, const char* code, const char* tag WG_DEFAULT_ARG(nullptr));

/**
* Set a function to be called when a programmer error occurs.
*
* @param callback The function to call or nullptr to use abort() instead.
* @param userdata User data to pass to the callback.
*/
WG_DLL_EXPORT void Wg_SetErrorCallback(Wg_ErrorCallback callback, void* userdata);

/**
* Get the current error string.
* 
* @param context The relevant context.
* @return The current error as a null terminated ASCII string.
*         This string is owned by the function and should not be freed.
*/
WG_DLL_EXPORT const char* Wg_GetErrorMessage(Wg_Context* context);

/**
* Get the current exception object.
*
* @param context The relevant context.
* @return The current exception object, or nullptr if there is no exception.
*/
WG_DLL_EXPORT Wg_Obj* Wg_GetCurrentException(Wg_Context* context);

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
WG_DLL_EXPORT void Wg_RaiseException(Wg_Context* context, const char* message WG_DEFAULT_ARG(nullptr), Wg_Obj* type WG_DEFAULT_ARG(nullptr));

/**
* Raise an existing exception object.
*
* Remarks:
* If an exception is already set, the old exception will be overwritten.
*
* @param context The relevant context.
* @param type The exception object to raise. The type must be a subclass of BaseException.
*/
WG_DLL_EXPORT void Wg_RaiseExceptionObject(Wg_Context* context, Wg_Obj* exception);

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
WG_DLL_EXPORT void Wg_RaiseArgumentCountError(Wg_Context* context, int given, int expected);

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
WG_DLL_EXPORT void Wg_RaiseArgumentTypeError(Wg_Context* context, int argIndex, const char* expected);

/**
* Raise a AttributeError with a formatted message.
*
* Remarks:
* If an exception is already set, the old exception will be overwritten.
*
* @param obj The object that does not have the attribute.
* @param attribute A null terminated ASCII string containing the attribute.
*/
WG_DLL_EXPORT void Wg_RaiseAttributeError(const Wg_Obj* obj, const char* attribute);

/**
* Raise a ZeroDivisionError.
*
* Remarks:
* If an exception is already set, the old exception will be overwritten.
*
* @param context The relevant context.
*/
WG_DLL_EXPORT void Wg_RaiseZeroDivisionError(Wg_Context* context);

/**
* Raise a IndexError.
*
* Remarks:
* If an exception is already set, the old exception will be overwritten.
*
* @param context The relevant context.
*/
WG_DLL_EXPORT void Wg_RaiseIndexError(Wg_Context* context);

/**
* Raise a KeyError.
*
* Remarks:
* If an exception is already set, the old exception will be overwritten.
*
* @param context The relevant context.
* @param key The key that caused the KeyError, or nullptr to leave unspecified.
*/
WG_DLL_EXPORT void Wg_RaiseKeyError(Wg_Context* context, Wg_Obj* key WG_DEFAULT_ARG(nullptr));

/**
* Raise a StopIteration to terminate iteration.
*
* Remarks:
* If an exception is already set, the old exception will be overwritten.
*
* @param context The relevant context.
*/
WG_DLL_EXPORT void Wg_RaiseStopIteration(Wg_Context* context);

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
WG_DLL_EXPORT void Wg_RaiseTypeError(Wg_Context* context, const char* message WG_DEFAULT_ARG(nullptr));

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
WG_DLL_EXPORT void Wg_RaiseValueError(Wg_Context* context, const char* message WG_DEFAULT_ARG(nullptr));

/**
* Raise a NameError.
*
* Remarks:
* If an exception is already set, the old exception will be overwritten.
*
* @param context The relevant context.
* @param name A null terminated ASCII string containing the name that was not found.
*/
WG_DLL_EXPORT void Wg_RaiseNameError(Wg_Context* context, const char* name);

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
WG_DLL_EXPORT Wg_Obj* Wg_IsInstance(const Wg_Obj* instance, Wg_Obj*const* types, int typesLen);

/**
* Clear the current exception.
*
* @param context The relevant context.
*/
WG_DLL_EXPORT void Wg_ClearCurrentException(Wg_Context* context);

/**
* Get the current configuration.
* 
* @param context The relevant context.
* @param out A pointer to a Wg_Config struct to receive the configuration information.
*/
WG_DLL_EXPORT void Wg_GetConfig(const Wg_Context* context, Wg_Config* out);

/**
* Set the current configuration.
* 
* @param context The relevant context.
* @param config The configuration information.
*/
WG_DLL_EXPORT void Wg_SetConfig(Wg_Context* context, const Wg_Config* config);

/**
* Get a global variable.
* 
* @param context The relevant context.
* @param name A null terminated ASCII string containing the name of the
*             global variable to retrieve.
* @return The value of the global variable, or nullptr if it does not exist.
*/
WG_DLL_EXPORT Wg_Obj* Wg_GetGlobal(Wg_Context* context, const char* name);

/**
* Set a global variable.
* 
* @param context The relevant context.
* @param name A null terminated ASCII string containing the name of the
*        global variable to set.
* @param value The value to set the global variable to.
*/
WG_DLL_EXPORT void Wg_SetGlobal(Wg_Context* context, const char* name, Wg_Obj* value);

/**
* Delete a global variable.
*
* Remarks:
* This function only unbinds a value from a global name
* and does not free the associated value nor call __del__().
* If the previously bound value has no references, Wg_CollectGarbage()
* can be used to destroy the object and free the memory.
* 
* @param context The relevant context.
* @param name A null terminated ASCII string containing the name of the
*        global variable to set.
*/
WG_DLL_EXPORT void Wg_DeleteGlobal(Wg_Context* context, const char* name);

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
WG_DLL_EXPORT void Wg_Print(const Wg_Context* context, const char* message, int len);

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
WG_DLL_EXPORT void Wg_PrintString(const Wg_Context* context, const char* message);

/**
* Run the garbage collector and free all unreachable objects.
* 
* @param context The relevant context.
*/
WG_DLL_EXPORT void Wg_CollectGarbage(Wg_Context* context);

/**
* Protect an object from being garbage collected.
* 
* Remarks:
* Call Wg_UnprotectObject() to allow the object to be collected again.
* 
* @param obj the object to protect.
*/
WG_DLL_EXPORT void Wg_ProtectObject(const Wg_Obj* obj);

/**
* Allow a object protected with Wg_ProtectObject() to be garbage collected again.
*
* @param obj the object to unprotect.
*/
WG_DLL_EXPORT void Wg_UnprotectObject(const Wg_Obj* obj);

/**
* Make a parent object reference a child object.
* 
* Remarks:
* This tells the garbage collector to preserve the child object if
* the parent object is still reachable. It is not necessary to
* call Wg_UnlinkReference() after calling this function.
* 
* @param parent the parent object.
* @param child the child object being.
*/
WG_DLL_EXPORT void Wg_LinkReference(Wg_Obj* parent, Wg_Obj* child);

/**
* Remove a reference created with Wg_LinkReference().
* 
* @param parent the parent object.
* @param child the child object.
*/
WG_DLL_EXPORT void Wg_UnlinkReference(Wg_Obj* parent, Wg_Obj* child);

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
WG_DLL_EXPORT Wg_Obj* Wg_CreateNone(Wg_Context* context);

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
WG_DLL_EXPORT Wg_Obj* Wg_CreateBool(Wg_Context* context, bool value WG_DEFAULT_ARG(false));

/**
* Instantiate an integer object.
*
* Remarks:
* Call Wg_GetCurrentException() or Wg_GetErrorMessage() to get error information.
*
* @param context The relevant context.
* @param value The value of the object.
* @return The instantiated object, or nullptr on failure.
*/
WG_DLL_EXPORT Wg_Obj* Wg_CreateInt(Wg_Context* context, Wg_int value WG_DEFAULT_ARG(0));

/**
* Instantiate a float object.
*
* Remarks:
* Call Wg_GetCurrentException() or Wg_GetErrorMessage() to get error information.
*
* @param context The relevant context.
* @param value The value of the object.
* @return The instantiated object, or nullptr on failure.
*/
WG_DLL_EXPORT Wg_Obj* Wg_CreateFloat(Wg_Context* context, Wg_float value WG_DEFAULT_ARG(0));

/**
* Instantiate a string object.
*
* Remarks:
* Call Wg_GetCurrentException() or Wg_GetErrorMessage() to get error information.
*
* @param context The relevant context.
* @param value A null terminated ASCII string, or nullptr for an empty string.
* @return The instantiated object, or nullptr on failure.
*/
WG_DLL_EXPORT Wg_Obj* Wg_CreateString(Wg_Context* context, const char* value WG_DEFAULT_ARG(nullptr));

/**
* Instantiate a tuple object.
*
* Remarks:
* Call Wg_GetCurrentException() or Wg_GetErrorMessage() to get error information.
*
* @param context The relevant context.
* @param argv A pointer to an array of objects to initialise the tuple with.
*             This can be nullptr if argc is 0.
* @param argc The number of objects to initialise the tuple with.
* @return The instantiated object, or nullptr on failure.
*/
WG_DLL_EXPORT Wg_Obj* Wg_CreateTuple(Wg_Context* context, Wg_Obj** argv, int argc);

/**
* Instantiate a list object.
*
* Remarks:
* Call Wg_GetCurrentException() or Wg_GetErrorMessage() to get error information.
*
* @param context The relevant context.
* @param argv A pointer to an array of objects to initialise the list with.
*             This can be nullptr if argc is 0.
* @param argc The number of objects to initialise the list with.
* @return The instantiated object, or nullptr on failure.
*/
WG_DLL_EXPORT Wg_Obj* Wg_CreateList(Wg_Context* context, Wg_Obj** argv WG_DEFAULT_ARG(nullptr), int argc WG_DEFAULT_ARG(0));

/**
* Instantiate a dictionary object.
*
* Remarks:
* Call Wg_GetCurrentException() or Wg_GetErrorMessage() to get error information.
*
* @param context The relevant context.
* @param keys A pointer to an array of keys to initialise the dictionary with.
*             This can be nullptr if argc is 0.
* @param values A pointer to an array of values to initialise the dictionary with.
*             This can be nullptr if argc is 0.
* @param argc The number of key value pairs to initialise the dictionary with.
* @return The instantiated object, or nullptr on failure.
*/
WG_DLL_EXPORT Wg_Obj* Wg_CreateDictionary(Wg_Context* context, Wg_Obj** keys WG_DEFAULT_ARG(nullptr), Wg_Obj** values WG_DEFAULT_ARG(nullptr), int argc WG_DEFAULT_ARG(0));

/**
* Instantiate a set object.
*
* Remarks:
* Call Wg_GetCurrentException() or Wg_GetErrorMessage() to get error information.
*
* @param context The relevant context.
* @param argv A pointer to an array of objects to initialise the set with.
*             This can be nullptr if argc is 0.
* @param argc The number of objects to initialise the set with.
* @return The instantiated object, or nullptr on failure.
*/
WG_DLL_EXPORT Wg_Obj* Wg_CreateSet(Wg_Context* context, Wg_Obj** argv WG_DEFAULT_ARG(nullptr), int argc WG_DEFAULT_ARG(0));

/**
* Instantiate a function object.
*
* Remarks:
* Call Wg_GetCurrentException() or Wg_GetErrorMessage() to get error information.
*
* @param context The relevant context.
* @param value A function description.
* @return The instantiated object, or nullptr on failure.
*/
WG_DLL_EXPORT Wg_Obj* Wg_CreateFunction(Wg_Context* context, const Wg_FuncDesc* value);

/**
* Instantiate a class object.
*
* Remarks:
* If no bases are specified, object is implicitly used as a base.
* Call Wg_GetCurrentException() or Wg_GetErrorMessage() to get error information.
*
* @param context The relevant context.
* @param name A null terminated ASCII string containing the name of the class.
*             Do not use a name beginning with two underscores.
* @param bases A pointer to an array of class objects to be used as a base.
*              This can be nullptr if baseCount is 0.
* @param baseCount The length of the bases array.
* @return The instantiated object, or nullptr on failure.
*/
WG_DLL_EXPORT Wg_Obj* Wg_CreateClass(Wg_Context* context, const char* name, Wg_Obj** bases, int baseCount);

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
WG_DLL_EXPORT void Wg_AddAttributeToClass(Wg_Obj* class_, const char* attribute, Wg_Obj* value);

/**
* Check if an object is None.
* 
* @param obj The object to inspect.
* @return True if the object is None, otherwise false.
*/
WG_DLL_EXPORT bool Wg_IsNone(const Wg_Obj* obj);

/**
* Check if an object is a boolean.
* 
* @param obj The object to inspect.
* @return True if the object is a boolean, otherwise false.
*/
WG_DLL_EXPORT bool Wg_IsBool(const Wg_Obj* obj);

/**
* Check if an object is an integer.
* 
* @param obj The object to inspect.
* @return True if the object is an integer, otherwise false.
*/
WG_DLL_EXPORT bool Wg_IsInt(const Wg_Obj* obj);

/**
* Check if an object is a float.
* 
* @param obj The object to inspect.
* @return True if the object is an integer or float, otherwise false.
*/
WG_DLL_EXPORT bool Wg_IsIntOrFloat(const Wg_Obj* obj);

/**
* Check if an object is a string.
* 
* @param obj The object to inspect.
* @return True if the object is a string, otherwise false.
*/
WG_DLL_EXPORT bool Wg_IsString(const Wg_Obj* obj);

/**
* Check if an object is a tuple.
* 
* @param obj The object to inspect.
* @return True if the object is a tuple, otherwise false.
*/
WG_DLL_EXPORT bool Wg_IsTuple(const Wg_Obj* obj);

/**
* Check if an object is a list.
* 
* @param obj The object to inspect.
* @return True if the object is a list, otherwise false.
*/
WG_DLL_EXPORT bool Wg_IsList(const Wg_Obj* obj);

/**
* Check if an object is a dictionary.
* 
* @param obj The object to inspect.
* @return True if the object is a dictionary, otherwise false.
*/
WG_DLL_EXPORT bool Wg_IsDictionary(const Wg_Obj* obj);

/**
* Check if an object is a set.
*
* @param obj The object to inspect.
* @return True if the object is a set, otherwise false.
*/
WG_DLL_EXPORT bool Wg_IsSet(const Wg_Obj* obj);

/**
* Check if an object is a function.
* 
* @param obj The object to inspect.
* @return True if the object is a function, otherwise false.
*/
WG_DLL_EXPORT bool Wg_IsFunction(const Wg_Obj* obj);

/**
* Check if an object is a class.
* 
* @param obj The object to inspect.
* @return True if the object is a class, otherwise false.
*/
WG_DLL_EXPORT bool Wg_IsClass(const Wg_Obj* obj);

/**
* Get the value from a boolean object.
* 
* @param obj The object to get the value from.
* @return The boolean value of the object.
*/
WG_DLL_EXPORT bool Wg_GetBool(const Wg_Obj* obj);

/**
* Get the value from an integer object.
*
* @param obj The object to get the value from.
* @return The integer value of the object.
*/
WG_DLL_EXPORT Wg_int Wg_GetInt(const Wg_Obj* obj);

/**
* Get the float value from an integer or float object.
*
* @param obj The object to get the value from.
* @return The float value of the object.
*/
WG_DLL_EXPORT Wg_float Wg_GetFloat(const Wg_Obj* obj);

/**
* Get the value from a string object.
*
* @param obj The object to get the value from.
* @return The string value of the object. This string is
*         owned by the function and should not be freed.
*/
WG_DLL_EXPORT const char* Wg_GetString(const Wg_Obj* obj);

/**
* Set the userdata for an object.
* 
* @param obj The object to set the userdata for.
* @param userdata The userdata to set.
*/
WG_DLL_EXPORT void Wg_SetUserdata(Wg_Obj* obj, void* userdata);

/**
* Try to get the userdata from an object.
*
* @param obj The object to get the value from.
* @param type A null terminated ASCII string containing the name of
*             the type to query.
* @param out A pointer to a void* to receive the userdata.
* @return True if obj matches the type given, otherwise false.
*/
WG_DLL_EXPORT bool Wg_TryGetUserdata(const Wg_Obj* obj, const char* type, void** out);

/**
* Get the finalizer of an object.
*
* @param obj The object to get the finalizer from.
* @param finalizer A finalizer description.
*/
WG_DLL_EXPORT void Wg_GetFinalizer(const Wg_Obj* obj, Wg_FinalizerDesc* finalizer);

/**
* Set the finalizer of an object.
*
* The finalizer is run when the object is garbage collected.
* Do not instantiate any objects in the finalizer.
* 
* @param obj The object to set the finalizer for.
* @param finalizer The received finalizer description.
*/
WG_DLL_EXPORT void Wg_SetFinalizer(Wg_Obj* obj, const Wg_FinalizerDesc* finalizer);

/**
* Check if an object contains an attribute.
*
* @param obj The object to get the attribute from.
* @param member A null terminated ASCII string containing the attribute name to get.
* @return The attribute value, or nullptr if the attribute does not exist.
*/
WG_DLL_EXPORT Wg_Obj* Wg_HasAttribute(Wg_Obj* obj, const char* member);

/**
* Get an attribute of an object.
* If the attribute does not exist, an AttributeError is raised.
* 
* @param obj The object to get the attribute from.
* @param member A null terminated ASCII string containing the attribute name to get.
* @return The attribute value, or nullptr if the attribute does not exist.
*/
WG_DLL_EXPORT Wg_Obj* Wg_GetAttribute(Wg_Obj* obj, const char* member);

/**
* Set an attribute of an object.
* 
* @param obj The object to set the attribute for.
* @param member A null terminated ASCII string containing the attribute name to set.
* @param value The value to set the attribute to.
*/
WG_DLL_EXPORT void Wg_SetAttribute(Wg_Obj* obj, const char* member, Wg_Obj* value);

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
WG_DLL_EXPORT Wg_Obj* Wg_GetAttributeFromBase(Wg_Obj* obj, const char* member, Wg_Obj* baseClass WG_DEFAULT_ARG(nullptr));

/**
* Iterate over an iterable object.
* 
* Remarks:
* This requires the iterable object to implement a __iter__() method
* which returns an iterator. The iterator must implement a __next__() method
* to advance the iterator and yield a value. When the iterator is exhausted,
* it should raise a StopIteration exception.
* Call Wg_GetCurrentException() or Wg_GetErrorMessage() to get error information.
* 
* @param obj The object to iterate over.
* @param userdata The userdata to be passed to the callback function.
* @param callback A callback function to be called for each value iterated.
*                 This callback should return true on a successful iteration,
*                 otherwise it should return false to signal an error and abort iteration.
* @return true on success, or false on failure.
*/
WG_DLL_EXPORT bool Wg_Iterate(Wg_Obj* obj, void* userdata, Wg_IterationCallback callback);

/**
* Unpack an iterable object into an array of Wg_Obj*.
*
* Remarks:
* If the number of objects yielded by the iterator does not match the
* count parameter, a ValueError is raised.
* This requires the iterable object to implement a __iter__() method
* which returns an iterator. The iterator must implement a __next__() method
* to advance the iterator and yield a value. When the iterator is exhausted,
* it should raise a StopIteration exception.
* Call Wg_GetCurrentException() or Wg_GetErrorMessage() to get error information.
*
* @param obj The object to iterate over.
* @param out A pointer to an array of Wg_Obj* to receive the values.
* @param count The length of the out array.
* @return true on success, or false on failure.
*/
WG_DLL_EXPORT bool Wg_Unpack(Wg_Obj* obj, Wg_Obj** out, int count);

/**
* Get the keyword arguments dictionary passed to the current function.
* 
* Remarks:
* Call Wg_GetCurrentException() or Wg_GetErrorMessage() to get error information.
* 
* @return The keywords arguments dictionary, or nullptr on failure.
*/
WG_DLL_EXPORT Wg_Obj* Wg_GetKwargs(Wg_Context* context);

/**
* Call a callable object.
* 
* Remarks:
* If the object is a function object then the function is called.
* If the object is a class object then the class is instantiated.
* Otherwise the object's __call__() method is called.
* The callable object and arguments are protected from garbage
* collection while the function call is being executed.
* Call Wg_GetCurrentException() or Wg_GetErrorMessage() to get error information.
* 
* @param callable The object to call.
* @param argv A pointer to an array of objects used as arguments to the function call.
*             If argc is 0 then this can be nullptr.
* @param argc The number of arguments to pass.
* @param kwargsDict A dictionary object containing the keyword arguments or nullptr if none.
* @return The return value of the callable, or nullptr on failure.
*/
WG_DLL_EXPORT Wg_Obj* Wg_Call(Wg_Obj* callable, Wg_Obj** argv, int argc, Wg_Obj* kwargsDict WG_DEFAULT_ARG(nullptr));

/**
* Call a method.
* 
* Remarks:
* Call Wg_GetCurrentException() or Wg_GetErrorMessage() to get error information.
* 
* @param obj The object to call the method on.
* @param member A null terminated ASCII string containing the name of the method to call.
* @param argv A pointer to an array of objects used as arguments to the method call.
*             If argc is 0 then this can be nullptr.
* @param argc The number of arguments to pass.
* @param kwargsDict A dictionary object containing the keyword arguments or nullptr if none.
* @return The return value of the method, or nullptr on failure.
*/
WG_DLL_EXPORT Wg_Obj* Wg_CallMethod(Wg_Obj* obj, const char* member, Wg_Obj** argv, int argc, Wg_Obj* kwargsDict WG_DEFAULT_ARG(nullptr));

/**
* Call a method from the base class.
*
* Remarks:
* Use this function if the method is shadowed by the derived class.
* If multiple bases contain this method, the first method found is called.
* Call Wg_GetCurrentException() or Wg_GetErrorMessage() to get error information.
*
* @param obj The object to call the method on.
* @param member A null terminated ASCII string containing the name of the method to call.
* @param argv A pointer to an array of objects used as arguments to the method call.
*             If argc is 0 then this can be nullptr.
* @param argc The number of arguments to pass.
* @param baseClass The base class to search in, or nullptr to search in all bases.
* @return The return value of the method, or nullptr on failure.
*/
WG_DLL_EXPORT Wg_Obj* Wg_CallMethodFromBase(Wg_Obj* obj, const char* member, Wg_Obj** argv, int argc, Wg_Obj* kwargsDict WG_DEFAULT_ARG(nullptr), Wg_Obj* baseClass WG_DEFAULT_ARG(nullptr));

/**
* Get the values from a **kwargs parameter.
*
* @param kwargs The kwargs parameter value. This must be a dictionary.
* @param keys A pointer to an array of null terminated ASCII strings containing
* 			  the keys to find.
* @param count The number of keys given in the keys array. This must be a positive number.
* @param out A pointer to an array to receive the values. The values are given
*			 in the same order as given to the keys parameter. For keys that
*			 were not found, nullptr is set instead of the dictionary value.
* @param out The object at the specified index, or nullptr if it is not found.
* @return False if an exception was raised due to an allocation failure, otherwise false.
*/
WG_DLL_EXPORT bool Wg_ParseKwargs(Wg_Obj* kwargs, const char*const* keys, int count, Wg_Obj** out);

/**
* Get at an index of an object. i.e. obj[index]
* 
* Remarks:
* This requires the object to implement a __getitem__() method.
* Call Wg_GetCurrentException() or Wg_GetErrorMessage() to get error information.
* 
* @param obj The object to index.
* @param index The index to get at.
* @return The object at the specified index, or nullptr on failure.
*/
WG_DLL_EXPORT Wg_Obj* Wg_GetIndex(Wg_Obj* obj, Wg_Obj* index);

/**
* Set at an index of an object i.e. obj[index] = value
* 
* This requires the object to implement a __setitem__() method.
* Call Wg_GetCurrentException() or Wg_GetErrorMessage() to get error information.
* 
* @param obj The object to index.
* @param index The index to set at.
* @param value The value to set.
* @return The new object at the specified index, or nullptr on failure.
*/
WG_DLL_EXPORT Wg_Obj* Wg_SetIndex(Wg_Obj* obj, Wg_Obj* index, Wg_Obj* value);

/**
* Call a unary operator on an object.
* 
* Remarks:
* See Wg_UnOp for a list of unary operations and the special methods that they call.
* Call Wg_GetCurrentException() or Wg_GetErrorMessage() to get error information.
* 
* @param op The unary operation to call.
* @param arg The object to call the operator on.
* @return The result of the operation, or nullptr on failure.
*/
WG_DLL_EXPORT Wg_Obj* Wg_UnaryOp(Wg_UnOp op, Wg_Obj* arg);

/**
* Call a binary operator on an object.
*
* Remarks:
* See Wg_BinOp for a list of binary operations and the special methods that they call.
* The logical and/or operators will short-circuit their truthy evaluation.
* Call Wg_GetCurrentException() or Wg_GetErrorMessage() to get error information.
*
* @param op The unary operation to call.
* @param arg The object to call the operator on.
* @return The result of the operation, or nullptr on failure.
*/
WG_DLL_EXPORT Wg_Obj* Wg_BinaryOp(Wg_BinOp op, Wg_Obj* lhs, Wg_Obj* rhs);

#undef WG_DEFAULT_ARG
#undef WG_DLL_EXPORT

#ifdef __cplusplus
} // extern "C"
#endif
