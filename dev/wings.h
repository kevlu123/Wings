/**
* @file wings.h
* 
* @brief The wings API.
*/

#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#define WG_DEFAULT_ARG(arg) = arg
#else
#include <stdbool.h>
#define WG_DEFAULT_ARG(arg)
#endif

#if defined(_WIN32) && defined(_WINDLL)
#define WG_DLL_EXPORT __declspec(dllexport)
#else
#define WG_DLL_EXPORT
#endif

/**
 * @brief An opaque type representing the state of an interpreter.
 * 
 * @see Wg_CreateContext, Wg_DestroyContext
*/
typedef struct Wg_Context Wg_Context;

/**
 * @brief An opaque type representing an object in the interpreter.
*/
typedef struct Wg_Obj Wg_Obj;

/**
* @brief The underlying data type of an integer object.
* 
* @see Wg_NewInt, Wg_GetInt
*/
typedef int64_t Wg_int;

/**
* @brief The unsigned version of Wg_int.
* 
* @see Wg_int
*/
typedef uint64_t Wg_uint;

/**
* @brief The underlying data type of a float object.
* 
* @see Wg_NewFloat, Wg_GetFloat
*/
typedef double Wg_float;

/**
* @brief The signature of a native function registered into the interpreter
*		 as a function object.
* 
* To get the keyword arguments passed to the function, call Wg_GetKwargs().
* 
* @param context The associated context.
* @param argv An array of objects passed to the function.
* @param argc The length of the argv array.
* @return The return value of the function call. If an exception was
*		  raised, NULL should be returned.
* 
* @see Wg_NewFunction, Wg_BindMethod
*/
typedef Wg_Obj* (*Wg_Function)(Wg_Context* context, Wg_Obj** argv, int argc);

/**
* @brief The signature of an object finalizer.
* 
* @warning Do not perform any object allocations in this function.
* 
* @param userdata The userdata specified when this callback was registered.
* 
* @see Wg_RegisterFinalizer
*/
typedef void (*Wg_Finalizer)(void* userdata);

/**
* @brief The signature of the print function used
*		 by the interpreter for printing.
* 
* @attention The message may not be null terminated and may contain null bytes in the middle.
* 
* @param message An array of bytes to be printed.
* @param len The length of the message in bytes.
* @param userdata The userdata specified when this callback was registered.
* 
* @see Wg_Config, Wg_Print, Wg_PrintString
*/
typedef void (*Wg_PrintFunction)(const char* message, int len, void* userdata);

/**
* @brief The signature of the fatal error callback.
* 
* @param message A null terminated string containing the error message.
* 
* @see Wg_SetErrorCallback
*/
typedef void (*Wg_ErrorCallback)(const char* message);

/**
* @brief The signature of an iteration callback used by Wg_Iterate().
* 
* The yielded object is protected from garbage collection
* for the duration of this function.
* 
* @param obj The object yielded by iteration.
* @param userdata The userdata specified in Wg_Iterate.
* @return A boolean indicating whether iteration should continue.
*		  A value of false does not necessarily mean an error occurred.
*/
typedef bool (*Wg_IterationCallback)(Wg_Obj* obj, void* userdata);

/**
* @brief The signature of a module loader.
* 
* @param context The associated context.
* @return A boolean indicating whether tthe module was loaded successfully.
* 
* @see Wg_RegisterModule, Wg_ImportModule, Wg_ImportFromModule, Wg_ImportAllFromModule
*/
typedef bool (*Wg_ModuleLoader)(Wg_Context* context);

#ifdef _WIN32
#pragma pack(push, 1)
#endif
#define WG_PACK_BEGIN 
/**
* @brief The configuration used to initialise an interpreter.
* 
* The configuration cannot be changed after the interpreter is initialised.
* 
* @see Wg_CreateContext
*/
typedef struct Wg_Config {
	/**
	* @brief Enables the os module and the global 'open' function.
	* 
	* This is set to false by default.
	* 
	* @warning Although this option can be enabled to prevent
	*			 scripts from directly accessing OS resources,
	*			 it does not provide a full sandbox. Untrusted scripts
	*			 should never be run with or without this option enabled.
	*/
	bool enableOSAccess;
	/**
	* @brief The maximum number of objects allowed to be allocated
	*		 at a time before a MemoryError will be raised.
	* 
	* This is set to 1 000 000 by default.
	*/
	int maxAlloc;
	/**
	* @brief The maximum recursion depth allowed before a RecursionError will be raised.
	* 
	* This is set to 50 by default.
	*/
	int maxRecursion;
	/**
	* @brief The 'aggressiveness' of the garbage collector. Higher means less aggressive.
	* 
	* The garbage collector runs when the number of allocated objects reaches
	* floor(gcRunFactor * lastObjectCountAfterGC).
	* 
	* This is set to 2.0 by default and must be >= 1.0.
	*/
	float gcRunFactor;
	/**
	* @brief The callback to be invoked when print is called in the interpreter.
	* If this is NULL, then print messages are discarded.
	* 
	* This is set to forward to std::cout by default.
	* 
	* @see Wg_Print, Wg_PrintString
	*/
	Wg_PrintFunction print;
	/**
	* @brief The userdata passed to the print callback.
	*/
	void* printUserdata;
	/**
	* @brief The path to search in when importing file modules.
	* The terminating directory separator is optional.
	* 
	* This is set to NULL by default which indicates the current working directory.
	*/
	const char* importPath;
	/**
	* @brief The commandline arguments passed to the interpreter.
	* If argc is 0, then this can be NULL.
	*/
	const char*const* argv;
	/**
	* @brief The length of the argv array.
	* If argc is 0, then a length 1 array with an empty string is implied.
	* 
	* This is set to 0 by default.
	*/
	int argc;
}
#ifndef _WIN32
__attribute__((packed))
#endif
Wg_Config;

#ifdef _WIN32
#pragma pack(pop)
#endif

/**
* @brief The unary operation to be used Wg_UnaryOp.
*/
typedef enum Wg_UnOp {
	/**
	* @brief The identity operator
	* 
	* Calls the \__pos__ special method.
	*/
	WG_UOP_POS,
	/**
	* @brief The unary minus operator
	*
	* Calls the \__neg__ special method.
	*/
	WG_UOP_NEG,
	/**
	* @brief The bitwise complement operator
	*
	* Calls the \__invert__ special method.
	*/
	WG_UOP_BITNOT,
	/**
	* @brief The logical not operator
	*
	* Calls the \__nonzero__ special method and inverts the result.
	* If \__nonzero__ returns a non boolean type, a TypeError is raised.
	*/
	WG_UOP_NOT,
	/**
	* @brief The hash operator
	* 
	* Calls the \__hash__ special method.
	* If \__hash__ returns a non integer type, a TypeError is raised.
	*/
	WG_UOP_HASH,
	/**
	* @brief The length operator
	*
	* Calls the \__len__ special method.
	* If \__len__ returns a non integer type, a TypeError is raised.
	*/
	WG_UOP_LEN,
	/**
	* @brief The bool conversion operator
	*
	* Calls the \__bool__ special method.
	* If \__bool__ returns a non boolean type, a TypeError is raised.
	*/
	WG_UOP_BOOL,
	/**
	* @brief The integer conversion operator
	*
	* Calls the \__int__ special method.
	* If \__int__ returns a non integer type, a TypeError is raised.
	*/
	WG_UOP_INT,
	/**
	* @brief The float operator operator
	*
	* Calls the \__float__ special method.
	* If \__float__ returns a non float type, a TypeError is raised.
	*/
	WG_UOP_FLOAT,
	/**
	* @brief The string conversion operator
	*
	* Calls the \__str__ special method.
	* If \__str__ returns a non string type, a TypeError is raised.
	*/
	WG_UOP_STR,
	/**
	* @brief The string representation operator
	*
	* Calls the \__repr__ special method.
	* If \__repr__ returns a non string type, a TypeError is raised.
	*/
	WG_UOP_REPR,
	/**
	* @brief The index conversion operator
	*
	* Calls the \__index__ special method.
	* If \__index__ returns a non integer type, a TypeError is raised.
	*/
	WG_UOP_INDEX,
} Wg_UnOp;

/**
* @brief The binary operation to be used by Wg_BinaryOp.
*/
typedef enum Wg_BinOp {
	/**
	* @brief The addition operator
	*
	* Calls the \__add__ special method.
	*/
	WG_BOP_ADD,
	/**
	* @brief The subtraction operator
	*
	* Calls the \__sub__ special method.
	*/
	WG_BOP_SUB,
	/**
	* @brief The multiplication operator
	*
	* Calls the \__mul__ special method.
	*/
	WG_BOP_MUL,
	/**
	* @brief The division operator
	*
	* Calls the \__truediv__ special method.
	*/
	WG_BOP_DIV,
	/**
	* @brief The floor division operator
	*
	* Calls the \__floordiv__ special method.
	*/
	WG_BOP_FLOORDIV,
	/**
	* @brief The modulo operator
	*
	* Calls the \__mod__ special method.
	*/
	WG_BOP_MOD,
	/**
	* @brief The power operator
	*
	* Calls the \__pow__ special method.
	*/
	WG_BOP_POW,
	/**
	* @brief The bitwise and operator
	*
	* Calls the \__and__ special method.
	*/
	WG_BOP_BITAND,
	/**
	* @brief The bitwise or operator
	*
	* Calls the \__or__ special method.
	*/
	WG_BOP_BITOR,
	/**
	* @brief The xor operator
	*
	* Calls the \__xor__ special method.
	*/
	WG_BOP_BITXOR,
	/**
	* @brief The logical and operator
	*
	* Calls the \__nonzero__ special method on both arguments
	* and returns the logical and of the result.
	* If either \__nonzero__ returns a non boolean type, a TypeError is raised.
	*/
	WG_BOP_AND,
	/**
	* @brief The logical or operator
	*
	* Calls the \__nonzero__ special method on both arguments
	* and returns the logical or of the result.
	* If either \__nonzero__ returns a non boolean type, a TypeError is raised.
	*/
	WG_BOP_OR,
	/**
	* @brief The bit left shift operator
	*
	* Calls the \__lshift__ special method.
	*/
	WG_BOP_SHL,
	/**
	* @brief The bit right shift operator
	*
	* Calls the \__rshift__ special method.
	*/
	WG_BOP_SHR,
	/**
	* @brief The in operator
	*
	* Calls the \__contains__ special method.
	* If \__contains__ returns a non boolean type, a TypeError is raised.
	*/
	WG_BOP_IN,
	/**
	* @brief The not in operator
	*
	* Calls the \__contains__ special method and inverts the result.
	* If \__contains__ returns a non boolean type, a TypeError is raised.
	*/
	WG_BOP_NOTIN,
	/**
	* @brief The equals operator
	*
	* Calls the \__eq__ special method.
	* If \__eq__ returns a non boolean type, a TypeError is raised.
	*/
	WG_BOP_EQ,
	/**
	* @brief The not equals operator
	*
	* Calls the \__ne__ special method.
	* If \__ne__ returns a non boolean type, a TypeError is raised.
	*/
	WG_BOP_NE,
	/**
	* @brief The less than operator
	*
	* Calls the \__lt__ special method.
	* If \__lt__ returns a non boolean type, a TypeError is raised.
	*/
	WG_BOP_LT,
	/**
	* @brief The less than or equals operator
	*
	* Calls the \__le__ special method.
	* If \__le__ returns a non boolean type, a TypeError is raised.
	*/
	WG_BOP_LE,
	/**
	* @brief The greater than operator
	*
	* Calls the \__gt__ special method.
	* If \__gt__ returns a non boolean type, a TypeError is raised.
	*/
	WG_BOP_GT,
	/**
	* @brief The greater than or equals operator
	*
	* Calls the \__ge__ special method.
	* If \__ge__ returns a non boolean type, a TypeError is raised.
	*/
	WG_BOP_GE,
} Wg_BinOp;

/**
* @brief The type exception to be raised by Wg_RaiseException.
*/
typedef enum Wg_Exc {
	/**
	* @brief BaseException
	*/
	WG_EXC_BASEEXCEPTION,
	/**
	* @brief SystemExit
	*/
	WG_EXC_SYSTEMEXIT,
	/**
	* @brief Exception
	*/
	WG_EXC_EXCEPTION,
	/**
	* @brief StopIteration
	*/
	WG_EXC_STOPITERATION,
	/**
	* @brief ArithmeticError
	*/
	WG_EXC_ARITHMETICERROR,
	/**
	* @brief OverflowError
	*/
	WG_EXC_OVERFLOWERROR,
	/**
	* @brief ZeroDivisionError
	*/
	WG_EXC_ZERODIVISIONERROR,
	/**
	* @brief AttributeError
	*/
	WG_EXC_ATTRIBUTEERROR,
	/**
	* @brief ImportError
	*/
	WG_EXC_IMPORTERROR,
	/**
	* @brief LookupError
	*/
	WG_EXC_LOOKUPERROR,
	/**
	* @brief IndexError
	*/
	WG_EXC_INDEXERROR,
	/**
	* @brief KeyError
	*/
	WG_EXC_KEYERROR,
	/**
	* @brief MemoryError
	*/
	WG_EXC_MEMORYERROR,
	/**
	* @brief NameError
	*/
	WG_EXC_NAMEERROR,
	/**
	* @brief OSError
	*/
	WG_EXC_OSERROR,
	/**
	* @brief IsADirectoryError
	*/
	WG_EXC_ISADIRECTORYERROR,
	/**
	* @brief RuntimeError
	*/
	WG_EXC_RUNTIMEERROR,
	/**
	* @brief NotImplementedError
	*/
	WG_EXC_NOTIMPLEMENTEDERROR,
	/**
	* @brief RecursionError
	*/
	WG_EXC_RECURSIONERROR,
	/**
	* @brief SyntaxError
	*/
	WG_EXC_SYNTAXERROR,
	/**
	* @brief TypeError
	*/
	WG_EXC_TYPEERROR,
	/**
	* @brief ValueError
	*/
	WG_EXC_VALUEERROR,
} Wg_Exc;

/**
* @brief Create an instance of an interpreter.
* 
* The returned context must be freed with Wg_DestroyContext().
* This function does not fail.
* 
* @param config The configuration to use, or NULL to use the default configuration.
* @return A newly created context.
*/
WG_DLL_EXPORT
Wg_Context* Wg_CreateContext(const Wg_Config* config WG_DEFAULT_ARG(nullptr));

/**
* @brief Free a context created with Wg_CreateContext().
* 
* @param context The context to free.
*/
WG_DLL_EXPORT
void Wg_DestroyContext(Wg_Context* context);

/**
* @brief Get the default configuration.
* 
* @param[out] config The returned default configuration.
* 
* @see Wg_CreateContext
*/
WG_DLL_EXPORT
void Wg_DefaultConfig(Wg_Config* config);

/**
* @brief Execute a script.
* 
* An exception will be raised if there are any errors
* and should be handled with Wg_ClearException() or propagated.
* 
* @param context The associated context.
* @param script The script to execute.
* @param prettyName The name to run the script under, or NULL to use a default name.
* @return A boolean indicating whether the script compiled and executed successfully.
* 
* @see Wg_GetException, Wg_GetErrorMessage
*/
WG_DLL_EXPORT
bool Wg_Execute(Wg_Context* context, const char* script, const char* prettyName WG_DEFAULT_ARG(nullptr));

/**
* @brief Execute a script containing an expression.
* 
* An exception will be raised if there are any errors
* and should be handled with Wg_ClearException() or propagated.
*
* @param context The associated context.
* @param script The expression to execute.
* @param prettyName The name to run the script under, or NULL to use a default name.
* @return The result of executing the expression, or NULL on failure.
* 
* @see Wg_GetException, Wg_GetErrorMessage
*/
WG_DLL_EXPORT
Wg_Obj* Wg_ExecuteExpression(Wg_Context* context, const char* script, const char* prettyName WG_DEFAULT_ARG(nullptr));

/**
* @brief Compile a script into a function object.
* 
* An exception will be raised if there are any errors
* and should be handled with Wg_ClearException() or propagated.
* 
* @param context The associated context.
* @param script The script to compile.
* @param prettyName The name to run the script under, or NULL to use a default name.
* @return A function object, or NULL on failure.
* 
* @see Wg_GetException, Wg_GetErrorMessage, Wg_Call
*/
WG_DLL_EXPORT
Wg_Obj* Wg_Compile(Wg_Context* context, const char* script, const char* prettyName WG_DEFAULT_ARG(nullptr));

/**
* @brief Compile an expression into a function object.
* 
* An exception will be raised if there are any errors
* and should be handled with Wg_ClearException() or propagated.
*
* @param context The associated context.
* @param script The expression to compile.
* @param prettyName The name to run the script under, or NULL to use a default name.
* @return A function object, or NULL on failure.
*
* @see Wg_GetException, Wg_GetErrorMessage, Wg_Call
*/
WG_DLL_EXPORT
Wg_Obj* Wg_CompileExpression(Wg_Context* context, const char* script, const char* prettyName WG_DEFAULT_ARG(nullptr));

/**
* @brief Set a callback for programmer errors.
* 
* This function is threadsafe.
*
* @param callback The callback to use, or NULL to default to abort().
*/
WG_DLL_EXPORT
void Wg_SetErrorCallback(Wg_ErrorCallback callback);

/**
* @brief Get the current exception message.
* 
* @param context The associated context.
* @return The current error string.
* 
* @see Wg_GetException, Wg_RaiseException, Wg_RaiseExceptionClass, Wg_RaiseExceptionObject, Wg_ReraiseExceptionObject
*/
WG_DLL_EXPORT
const char* Wg_GetErrorMessage(Wg_Context* context);

/**
* @brief Get the current exception object.
*
* @param context The associated context.
* @return The current exception object, or NULL if there is no exception.
* 
* @see Wg_GetErrorMessage, Wg_RaiseException, Wg_RaiseExceptionClass, Wg_RaiseExceptionObject, Wg_ReraiseExceptionObject
*/
WG_DLL_EXPORT
Wg_Obj* Wg_GetException(Wg_Context* context);

/**
* @brief Create and raise an exception.
*
* If an exception is already set, the old exception will be overwritten.
*
* @param context The associated context.
* @param type The exception type.
* @param message The error message, or NULL for an empty string.
* 
* @see Wg_RaiseExceptionClass, Wg_RaiseExceptionObject, Wg_ReraiseExceptionObject
*/
WG_DLL_EXPORT
void Wg_RaiseException(Wg_Context* context, Wg_Exc type, const char* message WG_DEFAULT_ARG(nullptr));

/**
* @brief Create and raise an exception using a class object.
* 
* The class must derive from BaseException, otherwise a TypeError is raised.
* 
* If an exception is already set, the old exception will be overwritten.
* 
* @param klass The exception class.
* @param message The error message, or NULL for an empty string.
* 
* @see Wg_RaiseException, Wg_RaiseExceptionObject, Wg_ReraiseExceptionObject
*/
WG_DLL_EXPORT
void Wg_RaiseExceptionClass(Wg_Obj* klass, const char* message WG_DEFAULT_ARG(nullptr));

/**
* @brief Raise an existing exception object.
* 
* The object's type must derive from BaseException, otherwise a TypeError is raised.
*
* If an exception is already set, the old exception will be overwritten.
*
* @param obj The exception object to raise. The type must be a subclass of BaseException.
* 
* @see Wg_RaiseException, Wg_RaiseExceptionClass, Wg_ReraiseExceptionObject
*/
WG_DLL_EXPORT
void Wg_RaiseExceptionObject(Wg_Obj* obj);

/**
* @brief Raise an existing exception object without affecting the stack trace.
*
* The object's type must derive from BaseException, otherwise a TypeError is raised.
*
* If an exception is already set, the old exception will be overwritten.
*
* @param obj The exception object to raise. The type must be a subclass of BaseException.
*
* @see Wg_RaiseException, Wg_RaiseExceptionClass, Wg_RaiseExceptionObject
*/
WG_DLL_EXPORT
void Wg_ReraiseExceptionObject(Wg_Obj* obj);

/**
* @brief Raise a TypeError with a formatted message.
*
* If an exception is already set, the old exception will be overwritten.
*
* @param context The associated context.
* @param given The number of arguments given.
* @param expected The number of arguments expected, or -1 if this is not a fixed number.
* 
* @see Wg_RaiseException, Wg_RaiseExceptionClass, Wg_RaiseExceptionObject
*/
WG_DLL_EXPORT
void Wg_RaiseArgumentCountError(Wg_Context* context, int given, int expected);

/**
* @brief Raise a TypeError with a formatted message.
*
* If an exception is already set, the old exception will be overwritten.
*
* @param context The associated context.
* @param index The parameter index of the invalid argument.
* @param expected A string describing the expected type.
* 
* @see Wg_RaiseException, Wg_RaiseExceptionClass, Wg_RaiseExceptionObject
*/
WG_DLL_EXPORT
void Wg_RaiseArgumentTypeError(Wg_Context* context, int index, const char* expected);

/**
* @brief Raise a AttributeError with a formatted message.
*
* If an exception is already set, the old exception will be overwritten.
*
* @param obj The object missing the attribute.
* @param attribute The missing attribute.
* 
* @see Wg_RaiseException, Wg_RaiseExceptionClass, Wg_RaiseExceptionObject
*/
WG_DLL_EXPORT
void Wg_RaiseAttributeError(const Wg_Obj* obj, const char* attribute);

/**
* @brief Raise a KeyError with a formatted message.
*
* If an exception is already set, the old exception will be overwritten.
*
* @param context The associated context.
* @param key The key that caused the KeyError, or NULL to leave unspecified.
* 
* @see Wg_RaiseException, Wg_RaiseExceptionClass, Wg_RaiseExceptionObject
*/
WG_DLL_EXPORT
void Wg_RaiseKeyError(Wg_Context* context, Wg_Obj* key WG_DEFAULT_ARG(nullptr));

/**
* @brief Raise a NameError with a formatted message.
*
* If an exception is already set, the old exception will be overwritten.
*
* @param context The associated context.
* @param name The name that was not found.
* 
* @see Wg_RaiseException, Wg_RaiseExceptionClass, Wg_RaiseExceptionObject
*/
WG_DLL_EXPORT
void Wg_RaiseNameError(Wg_Context* context, const char* name);

/**
* @brief Clear the current exception.
*
* @param context The associated context.
*/
WG_DLL_EXPORT
void Wg_ClearException(Wg_Context* context);

/**
* @brief Get the context associated with an object.
* 
* @param obj The object.
* @return The associated context.
*/

WG_DLL_EXPORT
Wg_Context* Wg_GetContextFromObject(Wg_Obj* obj);

/**
* @brief Check if an object's class derives from any of the specified classes.
* 
* @param instance The object to be checked.
* @param types An array of class objects to be checked against.
*              This can be NULL if typesLen is 0.
* @param typesLen The length of the types array.
* @return The first subclass matched, or NULL if the object's
*         class does not derive from any of the specified classes.
*/
WG_DLL_EXPORT
Wg_Obj* Wg_IsInstance(const Wg_Obj* instance, Wg_Obj*const* types, int typesLen);

/**
* @brief Get a global variable in the current module namespace.
* 
* @param context The associated context.
* @param name The name of the global variable.
* @return The value of the global variable, or NULL if it does not exist.
* 
* @see Wg_SetGlobal
*/
WG_DLL_EXPORT
Wg_Obj* Wg_GetGlobal(Wg_Context* context, const char* name);

/**
* @brief Set a global variable in the current module namespace.
* 
* @param context The associated context.
* @param name The name of the global variable.
* @param value The value to set.
* 
* @see Wg_GetGlobal
*/
WG_DLL_EXPORT
void Wg_SetGlobal(Wg_Context* context, const char* name, Wg_Obj* value);

/**
* @brief Print a message.
* 
* This function uses the print function specified in the configuration.
* 
* @param context The associated context.
* @param message A byte array containing the message.
*				 This can NULL if len is 0.
* @param len The length of the message byte array.
* 
* @see Wg_PrintString, Wg_Config
*/
WG_DLL_EXPORT
void Wg_Print(const Wg_Context* context, const char* message, int len);

/**
* @brief Print a string message.
*
* This function uses the print function specified in the configuration.
*
* @param context The associated context.
* @param message A null terminated message string.
* 
* @see Wg_Print, Wg_Config
*/
WG_DLL_EXPORT
void Wg_PrintString(const Wg_Context* context, const char* message);

/**
* @brief Force run the garbage collector and free all unreachable objects.
* 
* @param context The associated context.
*/
WG_DLL_EXPORT
void Wg_CollectGarbage(Wg_Context* context);

/**
* @brief Increment the reference count of an object.
* A positive reference count prevents the object from being garbage collected.
* 
* @param obj The object whose reference count is to be incremented.
* 
* @see Wg_DecRef
*/
WG_DLL_EXPORT
void Wg_IncRef(Wg_Obj* obj);

/**
* @brief Decrement the reference count of an object.
* A positive reference count prevents the object from being garbage collected.
*
* The reference count must not be negative.
* 
* @param obj The object whose reference count is to be decremented.
*
* @see Wg_IncRef
*/
WG_DLL_EXPORT
void Wg_DecRef(Wg_Obj* obj);

/**
* @brief Get the None singleton value.
* 
* @param context The associated context.
* @return The None singleton object.
*/
WG_DLL_EXPORT
Wg_Obj* Wg_None(Wg_Context* context);

/**
* @brief Instantiate a boolean object.
* 
* Unlike the other Wg_NewXXX functions, this function always succeeds.
* 
* @param context The associated context.
* @param value The value of the object.
* @return The instantiated object.
*/
WG_DLL_EXPORT
Wg_Obj* Wg_NewBool(Wg_Context* context, bool value WG_DEFAULT_ARG(false));

/**
* @brief Instantiate an integer object.
*
* @param context The associated context.
* @param value The value of the object.
* @return The instantiated object, or NULL on failure.
* 
* @see Wg_GetException, Wg_GetErrorMessage
*/
WG_DLL_EXPORT
Wg_Obj* Wg_NewInt(Wg_Context* context, Wg_int value WG_DEFAULT_ARG(0));

/**
* @brief Instantiate a float object.
*
* @param context The associated context.
* @param value The value of the object.
* @return The instantiated object, or NULL on failure.
* 
* @see Wg_GetException, Wg_GetErrorMessage
*/
WG_DLL_EXPORT
Wg_Obj* Wg_NewFloat(Wg_Context* context, Wg_float value WG_DEFAULT_ARG(0));

/**
* @brief Instantiate a string object.
*
* @param context The associated context.
* @param value A null terminated string, or NULL for an empty string.
* @return The instantiated object, or NULL on failure.
* 
* @see Wg_GetException, Wg_GetErrorMessage
*/
WG_DLL_EXPORT
Wg_Obj* Wg_NewString(Wg_Context* context, const char* value WG_DEFAULT_ARG(nullptr));

/**
* @brief Instantiate a string object from a buffer.
*
* @param context The associated context.
* @param buffer The buffer.
* @param len The length of the buffer.
* @return The instantiated object, or NULL on failure.
* 
* @see Wg_GetException, Wg_GetErrorMessage
*/
WG_DLL_EXPORT
Wg_Obj* Wg_NewStringBuffer(Wg_Context* context, const char* buffer, int len);

/**
* @brief Instantiate a tuple object.
*
* @param context The associated context.
* @param argv An array of objects to initialise the tuple with.
*             This can be NULL if argc is 0.
* @param argc The length of the argv array.
* @return The instantiated object, or NULL on failure.
* 
* @see Wg_GetException, Wg_GetErrorMessage
*/
WG_DLL_EXPORT
Wg_Obj* Wg_NewTuple(Wg_Context* context, Wg_Obj** argv, int argc);

/**
* @brief Instantiate a list object.
*
* @param context The associated context.
* @param argv An array of objects to initialise the list with.
*             This can be NULL if argc is 0.
* @param argc The length of the argv array.
* @return The instantiated object, or NULL on failure.
* 
* @see Wg_GetException, Wg_GetErrorMessage
*/
WG_DLL_EXPORT
Wg_Obj* Wg_NewList(Wg_Context* context, Wg_Obj** argv WG_DEFAULT_ARG(nullptr), int argc WG_DEFAULT_ARG(0));

/**
* @brief Instantiate a dictionary object.
*
* The keys must be hashable, otherwise a TypeError is raised.
* 
* @param context The associated context.
* @param keys An array of keys to initialise the dictionary with.
*             This can be NULL if argc is 0.
* @param values An array of values to initialise the dictionary with.
*             This can be NULL if argc is 0.
* @param len The length of the keys and values arrays.
* @return The instantiated object, or NULL on failure.
* 
* @see Wg_GetException, Wg_GetErrorMessage
*/
WG_DLL_EXPORT
Wg_Obj* Wg_NewDictionary(Wg_Context* context, Wg_Obj** keys WG_DEFAULT_ARG(nullptr), Wg_Obj** values WG_DEFAULT_ARG(nullptr), int len WG_DEFAULT_ARG(0));

/**
* @brief Instantiate a set object.
*
* @param context The associated context.
* @param argv An array of objects to initialise the set with.
*             This can be NULL if argc is 0.
* @param argc The length of the argv array.
* @return The instantiated object, or NULL on failure.
* 
* @see Wg_GetException, Wg_GetErrorMessage
*/
WG_DLL_EXPORT
Wg_Obj* Wg_NewSet(Wg_Context* context, Wg_Obj** argv WG_DEFAULT_ARG(nullptr), int argc WG_DEFAULT_ARG(0));

/**
* @brief Instantiate a function object.
* 
* To get the userdata in the function, call Wg_GetFunctionUserdata().
* To get the keyword arguments passed to the function, call Wg_GetKwargs().
* 
* @param context The associated context.
* @param fptr The native function to be bound.
* @param userdata The userdata to pass to the function when it is called.
* @param prettyName The name of the function, or NULL to use a default name.
* @return The instantiated object, or NULL on failure.
* 
* @see Wg_GetException, Wg_GetErrorMessage, Wg_BindMethod
*/
WG_DLL_EXPORT
Wg_Obj* Wg_NewFunction(Wg_Context* context, Wg_Function fptr, void* userdata, const char* prettyName WG_DEFAULT_ARG(nullptr));

/**
* @brief Instantiate a function object and bind it to a class.
* 
* To get the userdata in the function, call Wg_GetFunctionUserdata().
* To get the keyword arguments passed to the function, call Wg_GetKwargs().
* 
* @note Existing instances of the class will gain the new method.
*
* @param klass The class to bind the method to.
* @param name The name of the method.
* @param fptr The native function to be bound.
* @param userdata The userdata to pass to the function when it is called.
* @return The instantiated object, or NULL on failure.
*
* @see Wg_GetException, Wg_GetErrorMessage, Wg_BindMethod
*/
WG_DLL_EXPORT
Wg_Obj* Wg_BindMethod(Wg_Obj* klass, const char* name, Wg_Function fptr, void* userdata);

/**
* @brief Instantiate a class object.
* 
* Methods can be added later with Wg_BindMethod() and data members can be added
* to instances inside the \__init__ method with Wg_SetAttribute().
*
* If no bases are specified, the object class is implicitly used as a base.
*
* The name of the class cannot begin with two underscores.
* 
* @param context The associated context.
* @param name The name of the class.
* @param bases An array of class objects to be used as a base.
*              This can be NULL if basesLen is 0.
* @param basesLen The length of the bases array.
* @return The instantiated class object, or NULL on failure.
* 
* @see Wg_GetException, Wg_GetErrorMessage
*/
WG_DLL_EXPORT
Wg_Obj* Wg_NewClass(Wg_Context* context, const char* name, Wg_Obj** bases, int basesLen);

/**
* @brief Check if an object is None.
* 
* @param obj The object to inspect.
* @return True if the object is None, otherwise false.
*/
WG_DLL_EXPORT
bool Wg_IsNone(const Wg_Obj* obj);

/**
* @brief Check if an object is a boolean.
* 
* @param obj The object to inspect.
* @return True if the object is a boolean, otherwise false.
*/
WG_DLL_EXPORT
bool Wg_IsBool(const Wg_Obj* obj);

/**
* @brief Check if an object is an integer.
* 
* @param obj The object to inspect.
* @return True if the object is an integer, otherwise false.
*/
WG_DLL_EXPORT
bool Wg_IsInt(const Wg_Obj* obj);

/**
* @brief Check if an object is a float.
* 
* @param obj The object to inspect.
* @return True if the object is an integer or float, otherwise false.
*/
WG_DLL_EXPORT
bool Wg_IsIntOrFloat(const Wg_Obj* obj);

/**
* @brief Check if an object is a string.
* 
* @param obj The object to inspect.
* @return True if the object is a string, otherwise false.
*/
WG_DLL_EXPORT
bool Wg_IsString(const Wg_Obj* obj);

/**
* @brief Check if an object is a tuple.
* 
* @param obj The object to inspect.
* @return True if the object is a tuple, otherwise false.
*/
WG_DLL_EXPORT
bool Wg_IsTuple(const Wg_Obj* obj);

/**
* @brief Check if an object is a list.
* 
* @param obj The object to inspect.
* @return True if the object is a list, otherwise false.
*/
WG_DLL_EXPORT
bool Wg_IsList(const Wg_Obj* obj);

/**
* @brief Check if an object is a dictionary.
* 
* @param obj The object to inspect.
* @return True if the object is a dictionary, otherwise false.
*/
WG_DLL_EXPORT
bool Wg_IsDictionary(const Wg_Obj* obj);

/**
* @brief Check if an object is a set.
*
* @param obj The object to inspect.
* @return True if the object is a set, otherwise false.
*/
WG_DLL_EXPORT
bool Wg_IsSet(const Wg_Obj* obj);

/**
* @brief Check if an object is a function.
* 
* @param obj The object to inspect.
* @return True if the object is a function, otherwise false.
*/
WG_DLL_EXPORT
bool Wg_IsFunction(const Wg_Obj* obj);

/**
* @brief Check if an object is a class.
* 
* @param obj The object to inspect.
* @return True if the object is a class, otherwise false.
*/
WG_DLL_EXPORT
bool Wg_IsClass(const Wg_Obj* obj);

/**
* @brief Get the value from a boolean object.
* 
* @param obj The object to get the value from.
* @return The boolean value of the object.
*/
WG_DLL_EXPORT
bool Wg_GetBool(const Wg_Obj* obj);

/**
* @brief Get the value from an integer object.
*
* @param obj The object to get the value from.
* @return The integer value of the object.
*/
WG_DLL_EXPORT
Wg_int Wg_GetInt(const Wg_Obj* obj);

/**
* @brief Get the float value from an integer or float object.
*
* @param obj The object to get the value from.
* @return The float value of the object.
*/
WG_DLL_EXPORT
Wg_float Wg_GetFloat(const Wg_Obj* obj);

/**
* @brief Get the value from a string object.
*
* @note The string is always null terminated. If null bytes are
* expected to appear in the middle of the string, the length parameter
* can be used to get the true length of the string.
* 
* @param obj The object to get the value from.
* @param[out] len The length of the string. This parameter may be NULL.
* @return The string value of the object.
*/
WG_DLL_EXPORT
const char* Wg_GetString(const Wg_Obj* obj, int* len WG_DEFAULT_ARG(nullptr));

/**
* @brief Set the userdata for an object.
* 
* If the userdata requires cleanup (e.g. freeing memory), Wg_RegisterFinalizer() may be
* useful for automatically performing the cleanup when it the object is no longer needed.
* 
* @warning Only call this function on objects instantiated from user-created
* classes created with Wg_NewClass(). Do not set the userdata for builtin types.
* 
* @param obj The object to set the userdata for.
* @param userdata The userdata to set.
* 
* @see Wg_TryGetUserdata
*/
WG_DLL_EXPORT
void Wg_SetUserdata(Wg_Obj* obj, void* userdata);

/**
* @brief Get the userdata from an object if it is of the expected type.
*
* @param obj The object to get the value from.
* @param type The type to match.
* @param[out] userdata The userdata. This parameter may be NULL.
* @return A boolean indicating whether obj matches type.
* 
* @see Wg_SetUserdata, Wg_NewClass
*/
WG_DLL_EXPORT
bool Wg_TryGetUserdata(const Wg_Obj* obj, const char* type, void** userdata);

/**
* @brief Register a finalizer to run when an object is garbage collected.
* 
* Multiple finalizers may be registered.
* @warning Do not instantiate any objects in the finalizer.
* 
* @param obj The object to register the finalizer for.
* @param finalizer The finalizer function.
* @param userdata The userdata to pass to the finalizer function.
*/
WG_DLL_EXPORT
void Wg_RegisterFinalizer(Wg_Obj* obj, Wg_Finalizer finalizer, void* userdata WG_DEFAULT_ARG(nullptr));

/**
* @brief Check if an object has an attribute.
*
* @param obj The object to check.
* @param attribute The attribute to check.
* @return A boolean indicating whether the object has the attribute.
* 
* @see Wg_GetAttribute, Wg_GetAttributeFromBase, Wg_GetAttributeNoExcept, Wg_SetAttribute
*/
WG_DLL_EXPORT
bool Wg_HasAttribute(Wg_Obj* obj, const char* attribute);

/**
* @brief Get an attribute of an object.
* If the attribute does not exist, an AttributeError is raised.
* If the attribute is an unbound method object,
* a new method object is allocated with obj bound.
* If this allocation fails, a MemoryError is raised.
* 
* @param obj The object to get the attribute from.
* @param attribute The attribute to get.
* @return The attribute value, or NULL if the attribute
*		  does not exist or there is an error.
* 
* @see Wg_HasAttribute, Wg_GetAttributeFromBase, Wg_GetAttributeNoExcept, Wg_SetAttribute, Wg_GetException, Wg_GetErrorMessage
*/
WG_DLL_EXPORT
Wg_Obj* Wg_GetAttribute(Wg_Obj* obj, const char* attribute);

/**
* @brief Get an attribute of an object.
* Unlike, Wg_GetAttribute(), this function does not raise exceptions.
* @warning This function will not bind unbound method objects to obj.
*
* @param obj The object to get the attribute from.
* @param attribute The attribute to get.
* @return The attribute value, or NULL if the attribute does not exist.
*
* @see Wg_HasAttribute, Wg_GetAttribute, Wg_GetAttributeFromBase, Wg_SetAttribute
*/
WG_DLL_EXPORT
Wg_Obj* Wg_GetAttributeNoExcept(Wg_Obj* obj, const char* attribute);

/**
* @brief Set an attribute of an object.
* 
* @param obj The object to set the attribute for.
* @param attribute The attribute to set.
* @param value The attribute value.
* 
* @see Wg_HasAttribute, Wg_GetAttribute, Wg_GetAttributeNoExcept, Wg_GetAttributeFromBase
*/
WG_DLL_EXPORT
void Wg_SetAttribute(Wg_Obj* obj, const char* attribute, Wg_Obj* value);

/**
* @brief Get an attribute of an object, skipping attributes that belong to the most derived layer.
* 
* This is useful if the attribute is shadowed by the derived class.
* If multiple bases contain this attribute, the first attribute found is returned.
*
* @param obj The object to get the attribute from.
* @param attribute The attribute to get.
* @param baseClass The base class to search in, or NULL to search in all bases.
* @return The attribute value, or NULL if the attribute does not exist.
* 
* @see Wg_HasAttribute, Wg_GetAttribute, Wg_GetAttributeNoExcept, Wg_SetAttribute
*/
WG_DLL_EXPORT
Wg_Obj* Wg_GetAttributeFromBase(Wg_Obj* obj, const char* attribute, Wg_Obj* baseClass WG_DEFAULT_ARG(nullptr));

/**
* @brief Iterate over an iterable object.
* 
* The object must be iterable, otherwise an exception is raised.
* 
* The object is protected from garbage collection until the function returns.
* 
* @param obj The object to iterate over.
* @param userdata The userdata to be passed to the callback function.
* @param callback A function to be called for each value yielded by iteration.
*				  See Wg_IterationCallback for more details on this function.
* @return True on success, or false on failure.
* 
* @see Wg_GetException, Wg_GetErrorMessage
*/
WG_DLL_EXPORT
bool Wg_Iterate(Wg_Obj* obj, void* userdata, Wg_IterationCallback callback);

/**
* @brief Helper function to unpack an iterable object into an array of objects.
*
* If the number of objects yielded by the iterator does not match the
* count parameter, a ValueError is raised.
* 
* @param obj The object to iterate over.
* @param count The expected number of values.
* @param[out] values The unpacked objects.
* @return A boolean indicating success.
*/
WG_DLL_EXPORT
bool Wg_Unpack(Wg_Obj* obj, int count, Wg_Obj** values);

/**
* @brief Get the keyword arguments dictionary passed to the current function.
* 
* @note This function must be called inside a function bound with Wg_NewFunction() or Wg_BindMethod().
* @note This function can return NULL to indicate an empty dictionary.
* 
* @param context The associated context.
* @return The keywords arguments dictionary.
* 
* @see Wg_Call, Wg_CallMethod
*/
WG_DLL_EXPORT
Wg_Obj* Wg_GetKwargs(Wg_Context* context);

/**
* @brief Get the userdata associated with the current function.
*
* @note This function must be called inside a function bound with Wg_NewFunction() or Wg_BindMethod().
*
* @param context The associated context.
* @return The userdata associated with the current function.
* 
* @see Wg_NewFunction, Wg_BindMethod
*/
WG_DLL_EXPORT
void* Wg_GetFunctionUserdata(Wg_Context* context);

/**
* @brief Call a callable object.
* 
* Calling a class object will instantiate the class.
* 
* The kwargs parameter must be a dictionary with string keys,
* otherwise a TypeError is raised.
* 
* @param callable The object to call.
* @param argv An array of arguments to pass to the callable object.
*             If argc is 0 then this can be NULL.
* @param argc The length of the argv array.
* @param kwargs A dictionary object containing the keyword arguments or NULL if none.
* @return The return value of the callable, or NULL on failure.
* 
* @see Wg_CallMethod, Wg_GetException, Wg_GetErrorMessage
*/
WG_DLL_EXPORT
Wg_Obj* Wg_Call(Wg_Obj* callable, Wg_Obj** argv, int argc, Wg_Obj* kwargs WG_DEFAULT_ARG(nullptr));

/**
* @brief Call a method on a object.
* 
* This is equivalent to calling Wg_GetAttribute() and then Wg_Call().
* 
* The kwargs parameter must be a dictionary with string keys,
* otherwise a TypeError is raised.
* 
* @param obj The object to call the method on.
* @param method The method to call.
* @param argv An array of arguments to pass to the callable object.
*             If argc is 0 then this can be NULL.
* @param argc The length of the argv array.
* @param kwargs A dictionary object containing the keyword arguments or NULL if none.
* @return The return value of the callable, or NULL on failure.
* 
* @see Wg_Call, Wg_GetException, Wg_GetErrorMessage
*/
WG_DLL_EXPORT
Wg_Obj* Wg_CallMethod(Wg_Obj* obj, const char* method, Wg_Obj** argv, int argc, Wg_Obj* kwargs WG_DEFAULT_ARG(nullptr));

/**
* @brief Call a method on a object, skipping methods which belong to the most derived layer.
*
* This is equivalent to calling Wg_GetAttributeFromBase() and then Wg_Call().
* 
* This is useful if the method is shadowed by the derived class.
* If multiple bases contain this attribute, the first attribute found is returned.
*
* The kwargs parameter must be a dictionary with string keys,
* otherwise a TypeError is raised.
*
* @param obj The object to call the method on.
* @param method The method to call.
* @param argv An array of arguments to pass to the callable object.
*             If argc is 0 then this can be NULL.
* @param argc The length of the argv array.
* @param kwargs A dictionary object containing the keyword arguments or NULL if none.
* @param baseClass The base class to search in, or NULL to search in all bases.
* @return The return value of the callable, or NULL on failure.
*
* @see Wg_Call, Wg_GetException, Wg_GetErrorMessage
*/

WG_DLL_EXPORT
Wg_Obj* Wg_CallMethodFromBase(Wg_Obj* obj, const char* method, Wg_Obj** argv, int argc, Wg_Obj* kwargs WG_DEFAULT_ARG(nullptr), Wg_Obj* baseClass WG_DEFAULT_ARG(nullptr));

/**
* @brief Get the values from a kwargs parameter.
* 
* @note This function can be called with dictionaries
* other than the one returned by Wg_GetKwargs().
*
* @param dict The dictionary to get the values from.
*			  If this is NULL, then an empty dictionary is assumed.
* @param keys The keys to look up.
* @param keysLen The length of the keys array.
* @param[out] values The values in the dictionary corresponding to the keys.
*					 The values are given in the order that the keys are given
*					 and will be NULL for keys that were not found.
* @return A boolean indicating whether the operation was successful.
* 
* @see Wg_GetException, Wg_GetErrorMessage
*/
WG_DLL_EXPORT
bool Wg_ParseKwargs(Wg_Obj* dict, const char*const* keys, int keysLen, Wg_Obj** values);

/**
* @brief Index an object.
* 
* This calls the \__getitem__ method.
* 
* @param obj The object to index.
* @param index The index.
* @return The object at the specified index, or NULL on failure.
* 
* @see Wg_UnaryOp, Wg_BinaryOp, Wg_SetIndex, Wg_GetException, Wg_GetErrorMessage
*/
WG_DLL_EXPORT
Wg_Obj* Wg_GetIndex(Wg_Obj* obj, Wg_Obj* index);

/**
* @brief Set an index of an object.
* 
* This calls the \__setitem__ method.
* 
* @param obj The object to index.
* @param index The index.
* @param value The value to set.
* @return The result of the operation (usually None), or NULL on failure.
* 
* @see Wg_UnaryOp, Wg_BinaryOp, Wg_GetIndex, Wg_GetException, Wg_GetErrorMessage
*/
WG_DLL_EXPORT
Wg_Obj* Wg_SetIndex(Wg_Obj* obj, Wg_Obj* index, Wg_Obj* value);

/**
* @brief Perform a unary operation.
* 
* @param op The unary operation to perform.
* @param arg The operand.
* @return The result of the operation, or NULL on failure.
* 
* @see Wg_GetException, Wg_GetErrorMessage
*/
WG_DLL_EXPORT
Wg_Obj* Wg_UnaryOp(Wg_UnOp op, Wg_Obj* arg);

/**
* @brief Perform a binary operation.
*
* @note The logical and/or operators will short-circuit their truthy test.
*
* @param op The binary operation to perform.
* @param lhs The left hand side operand.
* @param rhs The right hand side operand.
* @return The result of the operation, or NULL on failure.
* 
* @see Wg_GetException, Wg_GetErrorMessage
*/
WG_DLL_EXPORT
Wg_Obj* Wg_BinaryOp(Wg_BinOp op, Wg_Obj* lhs, Wg_Obj* rhs);

/**
* @brief Register a callback to be called when a module with the given name is imported.
* In the callback, new members can be added to the module.
* 
* @param context The associated context.
* @param name The name of the module to register the callback for.
* @param loader The callback to register. The callback should return false on failure and otherwise true.
* 
* @see Wg_ImportModule, Wg_ImportFromModule, Wg_ImportAllFromModule
*/
WG_DLL_EXPORT
void Wg_RegisterModule(Wg_Context* context, const char* name, Wg_ModuleLoader loader);

/**
* @brief Import a module.
* 
* If the module has already been imported before, the module is not reloaded.
* 
* If the module is imported successfully, the module object
* is bound to a global variable with the same name (or the alias provided).
* 
* @attention If the names in the module are rebound to different objects
* after importing, the changes are not visible to outside modules.
* 
* @param context The associated context.
* @param module The name of the module to import.
* @param alias The alias to import the module under, or NULL to use the same name.
* @return The imported module object, or NULL on failure.
* 
* @see Wg_RegisterModule, Wg_ImportFromModule, Wg_ImportAllFromModule, Wg_GetException, Wg_GetErrorMessage
*/
WG_DLL_EXPORT
Wg_Obj* Wg_ImportModule(Wg_Context* context, const char* module, const char* alias WG_DEFAULT_ARG(nullptr));

/**
* @brief Import a specific name from a module.
*
* If the name is imported successfully, the object bound to the name
* is bound to a global variable with the same name (or the alias provided).
* 
* @attention If the names in the module are rebound to different objects
* after importing, the changes are not visible to outside modules.
*
* @param context The associated context.
* @param module The name of the module to import from.
* @param name The name to import.
* @param alias The alias to import the name under, or NULL to use the same name.
* @return The imported object, or NULL on failure.
* 
* @see Wg_RegisterModule, Wg_ImportModule, Wg_ImportAllFromModule, Wg_GetException, Wg_GetErrorMessage
*/
WG_DLL_EXPORT
Wg_Obj* Wg_ImportFromModule(Wg_Context* context, const char* module, const char* name, const char* alias WG_DEFAULT_ARG(nullptr));

/**
* @brief Import all names from a module.
*
* If the names are imported successfully, the objects bound to the names
* are bound to global variables with the same names.
* 
* @attention If the names in the module are rebound to different objects
* after importing, the changes are not visible to outside modules.
*
* @param context The associated context.
* @param module The name of the module to import.
* @return A boolean indicating if the names were imported successfully.
* 
* @see Wg_RegisterModule, Wg_ImportModule, Wg_ImportFromModule, Wg_GetException, Wg_GetErrorMessage
*/
WG_DLL_EXPORT
bool Wg_ImportAllFromModule(Wg_Context* context, const char* module);

#undef WG_DEFAULT_ARG
#undef WG_DLL_EXPORT

#ifdef __cplusplus
} // extern "C"
#endif
