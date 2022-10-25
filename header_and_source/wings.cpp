/**
* @file wings.h
* 
* @brief The wings API.
*/

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
	const char* const* argv;
	/**
	* @brief The length of the argv array.
	* If argc is 0, then a length 1 array with an empty string is implied.
	* 
	* This is set to 0 by default.
	*/
	int argc;
} Wg_Config;

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
* 
* @param config The configuration to use, or NULL to use the default configuration.
* @return A newly created context, or NULL on failure.
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
* @see Wg_GetException, Wg_RaiseException, Wg_RaiseExceptionClass, Wg_RaiseExceptionObject
*/
WG_DLL_EXPORT
const char* Wg_GetErrorMessage(Wg_Context* context);

/**
* @brief Get the current exception object.
*
* @param context The associated context.
* @return The current exception object, or NULL if there is no exception.
* 
* @see Wg_GetErrorMessage, Wg_RaiseException, Wg_RaiseExceptionClass, Wg_RaiseExceptionObject
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
* @see Wg_RaiseExceptionClass, Wg_RaiseExceptionObject
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
* @see Wg_RaiseException, Wg_RaiseExceptionObject
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
* @see Wg_RaiseException, Wg_RaiseExceptionClass
*/
WG_DLL_EXPORT
void Wg_RaiseExceptionObject(Wg_Obj* obj);

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
* @brief Get an attribute of an object if it exists.
*
* @param obj The object to get the attribute from.
* @param attribute The attribute to get.
* @return The attribute value, or NULL if the attribute does not exist.
* 
* @see Wg_GetAttribute, Wg_GetAttributeFromBase, Wg_SetAttribute
*/
WG_DLL_EXPORT
Wg_Obj* Wg_HasAttribute(Wg_Obj* obj, const char* attribute);

/**
* @brief Get an attribute of an object.
* If the attribute does not exist, an AttributeError is raised.
* 
* To get an attribute without raising an AttributeError, see Wg_HasAttribute().
* 
* @param obj The object to get the attribute from.
* @param attribute The attribute to get.
* @return The attribute value, or NULL if the attribute does not exist.
* 
* @see Wg_GetAttributeFromBase, Wg_SetAttribute, Wg_GetException, Wg_GetErrorMessage
*/
WG_DLL_EXPORT
Wg_Obj* Wg_GetAttribute(Wg_Obj* obj, const char* attribute);

/**
* @brief Set an attribute of an object.
* 
* @param obj The object to set the attribute for.
* @param attribute The attribute to set.
* @param value The attribute value.
* 
* @see Wg_HasAttribute, Wg_GetAttribute, Wg_GetAttributeFromBase
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
* @see Wg_HasAttribute, Wg_GetAttribute, Wg_GetAttributeFromBase
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
* 
* @param context The associated context.
* @return The keywords arguments dictionary, or NULL on failure.
* 
* @see Wg_Call, Wg_CallMethod, Wg_GetException, Wg_GetErrorMessage
*/
WG_DLL_EXPORT
Wg_Obj* Wg_GetKwargs(Wg_Context* context);

/**
* @brief Get the userdata associated with the current function.
*
* @note This function must be called inside a function bound with Wg_NewFunction() or Wg_BindMethod().
*
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

///////////////// Implementation ////////////////////////

#include <memory>

namespace wings {

	// Definitions may change later

	// Reference counted smart pointer.
	template <typename T>
	using RcPtr = std::shared_ptr<T>;

	template <typename T, typename... Args>
	RcPtr<T> MakeRcPtr(Args&&... args) {
		return std::make_shared<T>(std::forward<Args>(args)...);
	}
}


#include <string>
#include <vector>
#include <unordered_map>
#include <cstdlib>

namespace wings {

	struct AttributeTable {
		AttributeTable();
		AttributeTable(const AttributeTable&) = delete;
		AttributeTable(AttributeTable&&) = default;
		AttributeTable& operator=(const AttributeTable&) = delete;
		AttributeTable& operator=(AttributeTable&&) = default;
		
		Wg_Obj* Get(const std::string& name) const;
		Wg_Obj* GetFromBase(const std::string& name) const;
		void Set(const std::string& name, Wg_Obj* value);
		
		void AddParent(AttributeTable& parent);
		AttributeTable Copy();
		template <class Fn> void ForEach(Fn fn) const;
	private:		
		struct Table {
			Wg_Obj* Get(const std::string& name) const;
			template <class Fn> void ForEach(Fn fn) const;
			std::unordered_map<std::string, Wg_Obj*> entries;
			std::vector<RcPtr<Table>> parents;
		};

		void Mutate();

		RcPtr<Table> attributes;
		bool owned;
	};

	template <class Fn>
	void AttributeTable::ForEach(Fn fn) const {
		attributes->ForEach(fn);
	}

	template <class Fn>
	void AttributeTable::Table::ForEach(Fn fn) const {
		for (const auto& [_, val] : entries)
			fn(val);

		for (const auto& parent : parents)
			parent->ForEach(fn);
	}
}



namespace wings {

	AttributeTable::AttributeTable() :
		attributes(MakeRcPtr<Table>()),
		owned(true)
	{
	}

	Wg_Obj* AttributeTable::Get(const std::string& name) const {
		return attributes->Get(name);
	}

	Wg_Obj* AttributeTable::Table::Get(const std::string& name) const {
		auto it = entries.find(name);
		if (it != entries.end())
			return it->second;

		for (const auto& parent : parents)
			if (Wg_Obj* val = parent->Get(name))
				return val;

		return nullptr;
	}

	Wg_Obj* AttributeTable::GetFromBase(const std::string& name) const {
		for (const auto& parent : attributes->parents)
			if (Wg_Obj* val = parent->Get(name))
				return val;
		return nullptr;
	}

	void AttributeTable::Set(const std::string& name, Wg_Obj* value) {
		Mutate();
		attributes->entries[name] = value;
	}

	void AttributeTable::AddParent(AttributeTable& parent) {
		attributes->parents.push_back(parent.attributes);
	}

	AttributeTable AttributeTable::Copy() {
		AttributeTable copy;
		copy.attributes = attributes;
		copy.owned = false;
		return copy;
	}

	void AttributeTable::Mutate() {
		if (!owned) {
			attributes = MakeRcPtr<Table>(*attributes);
			owned = true;
		}
	}
}


namespace wings {
	bool ImportBuiltins(Wg_Context* context);
}

#include <stdint.h>
#include <optional>
#include <vector>
#include <stdexcept>
#include <utility>

/*
* The RelaxedSet and RelaxedMap are versions of std::unordered_set
* and std::unordered_map with more relaxed requirements.
* 
* Unlike the STL versions, an inconsistent hash or equality
* function will yield unspecified behaviour instead of
* undefined behaviour.
* Furthermore, the container can be modified while iterating
* through it. Doing so will yield unspecified but not undefined behaviour.
* 
* RelaxedMap will also iterate by insertion order. To achieve fast ordered iteration
* and O(1) insertion, deletion, and lookup, deletions do not shrink the
* underlying buffer.
* 
* If an exception is thrown from the hash or equality function,
* the container if left unmodified.
*/

namespace wings {

	template <class Key, class Hash, class Equal, class BucketItem>
	struct RelaxedHash {
	private:
		using Bucket = std::vector<BucketItem>;
		
	public:
		virtual void rehash(size_t count) = 0;
		
		RelaxedHash() : hasher(), equal(), buckets(1) {
		}

		bool contains(const Key& key) const {
			const Bucket* buck = nullptr;
			return get_item(key, &buck);
		}

		bool empty() const noexcept {
			return size() == 0;
		}

		size_t size() const noexcept {
			return mySize;
		}

		size_t bucket_count() const noexcept {
			return buckets.size();
		}

		size_t bucket(const Key& key) const {
			return hasher(key) % bucket_count();
		}

		size_t bucket_size(size_t n) noexcept {
			return buckets[n].size();
		}

		float load_factor() const noexcept {
			return (float)size() / bucket_count();
		}

		float max_load_factor() const noexcept {
			return maxLoadFactor;
		}

		void max_load_factor(float ml) noexcept {
			maxLoadFactor = ml;
		}

	protected:
		virtual BucketItem* get_item(const Key& key, Bucket** buck) = 0;
		virtual const BucketItem* get_item(const Key& key, const Bucket** buck) const = 0;
		
		void incr_size() {
			mySize++;
			if (load_factor() > max_load_factor())
				rehash(bucket_count() * 2 + 1);
		}

		void decr_size() {
			mySize--;
		}

		void clear_buckets() noexcept {
			for (auto& buck : buckets)
				buck.clear();
			mySize = 0;
		}
		
		Hash hasher;
		Equal equal;
		std::vector<Bucket> buckets;
		float maxLoadFactor = 1.0f;
		size_t mySize = 0;
	};

	template <class Key, class Hash = std::hash<Key>, class Equal = std::equal_to<Key>>
	struct RelaxedSet : RelaxedHash<Key, Hash, Equal, Key> {
	private:
		template <class Container>
		struct Iterator {
			Iterator(Container* container = nullptr, size_t bucketIndex = (size_t)-1, size_t itemIndex = (size_t)-1) :
				container(container), bucketIndex(bucketIndex), itemIndex(itemIndex) {
				Revalidate();
			}
			
			const Key& operator*() const {
				return container->buckets[bucketIndex][itemIndex];
			}

			const Key* operator->() const {
				return &container->buckets[bucketIndex][itemIndex];
			}

			Iterator& operator++() {
				itemIndex++;
				Revalidate();
				return *this;
			}

			bool operator==(const Iterator& rhs) const {
				return (!container && !rhs.container)
					|| (bucketIndex == rhs.bucketIndex
					    && itemIndex == rhs.itemIndex);
			}

			bool operator!=(const Iterator& rhs) const {
				return !(*this == rhs);
			}

			void Revalidate() {
				while (!CheckEnd() && itemIndex >= container->buckets[bucketIndex].size()) {
					bucketIndex++;
					itemIndex = 0;
				}
			}
		private:
			bool CheckEnd() {
				if (container && bucketIndex >= container->buckets.size())
					container = nullptr;
				return container == nullptr;
			}
			
			friend RelaxedSet;
			Container* container;
			size_t bucketIndex;
			size_t itemIndex;
		};
		
		using Bucket = std::vector<Key>;

	public:
		using iterator = Iterator<RelaxedSet>;
		using const_iterator = Iterator<const RelaxedSet>;

		void clear() noexcept {
			this->clear_buckets();
		}
		
		void insert(Key key) {
			Bucket* buck = nullptr;
			if (!this->get_item(key, &buck)) {
				buck->push_back(std::move(key));
				this->incr_size();
			}
		}

		const_iterator find(const Key& key) const {
			const Bucket* buck = nullptr;
			if (auto* item = this->get_item(key, &buck)) {
				return const_iterator{
					this,
					(size_t)(buck - this->buckets.data()),
					(size_t)(item - buck->data())
				};
			}
			return end();
		}

		void erase(iterator it) {
			auto& buck = this->buckets[it.bucketIndex];
			buck.erase(buck.begin() + it.itemIndex);
			this->decr_size();
		}

		void erase(const_iterator it) {
			erase(iterator{ this, it.bucketIndex, it.itemIndex });
		}

		void rehash(size_t count) override {
			auto oldBuckets = std::move(this->buckets);
			clear();
			this->buckets.resize(count);
			for (auto& oldBucket : oldBuckets) {
				for (auto& item : oldBucket) {
					Bucket* buck = nullptr;
					this->get_item(item, &buck);
					buck->push_back(std::move(item));
					this->mySize++;
				}
			}
		}

		const_iterator cbegin() const noexcept { return const_iterator(this, 0, 0); }
		const_iterator begin() const noexcept { return cbegin(); }
		iterator begin() noexcept { return iterator(this, 0, 0); }
		const_iterator cend() const noexcept { return const_iterator(); }
		const_iterator end() const noexcept { return cend(); }
		iterator end() noexcept { return iterator(); }
		
	protected:
		Key* get_item(const Key& key, Bucket** buck) override {
			auto& b = this->buckets[this->bucket(key)];
			*buck = &b;
			for (auto& item : b)
				if (this->equal(item, key))
					return &item;
			return nullptr;
		}

		const Key* get_item(const Key& key, const Bucket** buck) const override {
			auto& b = this->buckets[this->bucket(key)];
			*buck = &b;
			for (auto& item : b)
				if (this->equal(item, key))
					return &item;
			return nullptr;
		}
	};
	
	template <class Key, class Value, class Hash = std::hash<Key>, class Equal = std::equal_to<Key>>
	struct RelaxedMap : RelaxedHash<Key, Hash, Equal, size_t> {
	private:
		template <class Container>
		struct Iterator {
			Iterator(Container* container = nullptr, size_t index = (size_t)-1) :
				container(container), index(index) {
				Revalidate();
			}
			
			auto& operator*() const {
				return container->storage[index].value();
			}

			auto* operator->() const {
				return &container->storage[index].value();
			}

			Iterator& operator++() {
				index++;
				Revalidate();
				return *this;
			}

			bool operator==(const Iterator& rhs) const {
				return (!container && !rhs.container) || index == rhs.index;
			}

			bool operator!=(const Iterator& rhs) const {
				return !(*this == rhs);
			}

			void Revalidate() {
				while (!CheckEnd() && container->storage[index] == std::nullopt) {
					index++;
				}
			}
		private:
			bool CheckEnd() {
				if (container && index >= container->storage.size())
					container = nullptr;
				return container == nullptr;
			}

			friend RelaxedMap;
			Container* container;
			size_t index;
		};

		using Bucket = std::vector<size_t>;
		std::vector<std::optional<std::pair<const Key, Value>>> storage;
		
	public:
		using iterator = Iterator<RelaxedMap>;
		using const_iterator = Iterator<const RelaxedMap>;

		RelaxedMap() = default;
		RelaxedMap(RelaxedMap&&) = delete;
		RelaxedMap& operator=(RelaxedMap&&) = delete;

		void clear() noexcept {
			this->clear_buckets();
			storage.clear();
		}
		
		void insert(std::pair<const Key, Value> pair) {
			Bucket* buck = nullptr;
			if (!this->get_item(pair.first, &buck)) {
				buck->push_back(storage.size());
				storage.push_back(std::move(pair));
				this->incr_size();
			}
		}

		std::optional<Value> erase(const Key& key) {
			Bucket* buck = nullptr;
			if (auto* index = this->get_item(key, &buck)) {
				buck->erase(std::find(buck->begin(), buck->end(), *index));
				auto pair = std::move(storage[*index]);
				storage[*index] = std::nullopt;
				this->decr_size();
				return pair.value().second;
			}
			return std::nullopt;
		}

		std::pair<const Key, Value> pop() {
			size_t i = storage.size() - 1;
			while (storage[i] == std::nullopt)
				i--;
			const auto& key = storage[i].value().first;

			Bucket* buck = nullptr;
			auto* index = this->get_item(key, &buck);
			buck->erase(std::find(buck->begin(), buck->end(), *index));
			auto pair = std::move(storage[*index]);
			storage[*index] = std::nullopt;
			this->decr_size();
			return pair.value();
		}
		
		iterator find(const Key& key) {
			Bucket* buck = nullptr;
			if (auto* index = this->get_item(key, &buck))
				return iterator{ this, *index };
			return end();
		}

		const_iterator find(const Key& key) const {
			const Bucket* buck = nullptr;
			if (auto* index = this->get_item(key, &buck))
				return iterator{ this, *index };
			return end();
		}

		Value& at(const Key& key) {
			if (auto* value = try_at(key))
				return *value;
			throw std::out_of_range();
		}
		
		const Value& at(const Key& key) const {
			if (auto* value = try_at(key))
				return *value;
			throw std::out_of_range();
		}

		Value& operator[](const Key& key) {
			this->incr_size();
			Bucket* buck = nullptr;
			if (auto* index = this->get_item(key, &buck))
				return storage[*index].value().second;
			buck->push_back(storage.size());
			storage.push_back(std::pair<const Key, Value>({ key, Value() }));
			return storage.back().value().second;
		}

		void rehash(size_t count) override {
			this->buckets.clear();
			this->buckets.resize(count);
			for (size_t i = 0; i < storage.size(); i++) {
				if (storage[i] != std::nullopt) {
					Bucket* buck = nullptr;
					this->get_item(storage[i].value().first, &buck);
					buck->push_back(i);
				}
			}
		}

		const_iterator cbegin() const noexcept { return const_iterator(this, 0); }
		const_iterator begin() const noexcept { return cbegin(); }
		iterator begin() noexcept { return iterator(this, 0); }
		const_iterator cend() const noexcept { return const_iterator(); }
		const_iterator end() const noexcept { return cend(); }
		iterator end() noexcept { return iterator(); }

	protected:
		size_t* get_item(const Key& key, Bucket** buck) override {
			auto& b = this->buckets[this->bucket(key)];
			*buck = &b;
			for (auto& index : b)
				if (this->equal(storage[index].value().first, key))
					return &index;
			return nullptr;
		}

		const size_t* get_item(const Key& key, const Bucket** buck) const override {
			auto& b = this->buckets[this->bucket(key)];
			*buck = &b;
			for (auto& index : b)
				if (this->equal(storage[index].value().first, key))
					return &index;
			return nullptr;
		}
	};
}


#include <string>
#include <string_view>
#include <vector>
#include <deque>
#include <stack>
#include <unordered_map>
#include <array>
#include <memory>
#include <type_traits>
#include <cstdlib>
#include <atomic>
#include <random>

static_assert(sizeof(Wg_int) == sizeof(Wg_uint));

namespace wings {
	extern std::atomic<Wg_ErrorCallback> errorCallback;
	
	size_t Guid();
	std::string WObjTypeToString(const Wg_Obj* obj);
	void CallErrorCallback(const char* message);
	Wg_Obj* Alloc(Wg_Context* context);
	bool IsKeyword(std::string_view s);
	bool IsValidIdentifier(std::string_view s);
	Wg_Obj* Compile(Wg_Context* context, const char* code, const char* module, const char* prettyName, bool expr);
	Wg_Obj* Execute(Wg_Context* context, const char* code, const char* module);
	bool InitArgv(Wg_Context* context, const char* const* argv, int argc);
	void RegisterMethod(Wg_Obj* klass, const char* name, Wg_Function fptr);
	Wg_Obj* RegisterFunction(Wg_Context* context, const char* name, Wg_Function fptr);
	void AddAttributeToClass(Wg_Obj* klass, const char* attribute, Wg_Obj* value);

	struct LibraryInitException : std::exception {};

	struct Executor;

	template <class F, class T>
	void RegisterConstant(Wg_Context* context, const char* name, F f, T v) {
		Wg_Obj* obj = f(context, v);
		if (obj == nullptr)
			throw LibraryInitException();
		Wg_SetGlobal(context, name, obj);
	}

	template <class T>
	bool TryGetUserdata(Wg_Obj* obj, const char* type, T** out) {
		return Wg_TryGetUserdata(obj, type, (void**)out);
	}

	struct Rng {
		Rng();
		void Seed(Wg_int seed);
		Wg_float Rand();
		Wg_int Int(Wg_int minIncl, Wg_int maxIncl);
		Wg_float Float(Wg_float minIncl, Wg_float maxIncl);
		std::mt19937_64& Engine();
	private:
		std::mt19937_64 engine;
		std::uniform_real_distribution<Wg_float> dist;
	};
	
	struct WObjHasher {
		size_t operator()(Wg_Obj* obj) const;
	};

	struct WObjComparer {
		bool operator()(Wg_Obj* lhs, Wg_Obj* rhs) const;
	};

	using WDict = RelaxedMap<Wg_Obj*, Wg_Obj*, WObjHasher, WObjComparer>;
	using WSet = RelaxedSet<Wg_Obj*, WObjHasher, WObjComparer>;

	struct SourcePosition {
		size_t line = (size_t)-1;
		size_t column = (size_t)-1;
	};

	struct CodeError {
		bool good = true;
		SourcePosition srcPos{};
		std::string message;

		operator bool() const;
		std::string ToString() const;
		static CodeError Good();
		static CodeError Bad(std::string message, SourcePosition srcPos = {});
	};

	struct OwnedTraceFrame {
		SourcePosition srcPos;
		std::string lineText;
		std::string module;
		std::string func;
		bool syntaxError;
	};

	struct TraceFrame {
		SourcePosition srcPos;
		std::string_view lineText;
		std::string_view module;
		std::string_view func;
		bool syntaxError = false;
		OwnedTraceFrame ToOwned() const {
			return { srcPos, std::string(lineText), std::string(module), std::string(func), syntaxError };
		}
	};

	struct HashException : public std::exception {};

	struct Wg_ObjRef {
		Wg_ObjRef() : obj(nullptr) {}
		explicit Wg_ObjRef(Wg_Obj* obj) : obj(obj) { if (obj) Wg_IncRef(obj); }
		explicit Wg_ObjRef(Wg_ObjRef&& other) noexcept : obj(other.obj) { other.obj = nullptr; }
		Wg_ObjRef& operator=(Wg_ObjRef&& other) noexcept { obj = other.obj; other.obj = nullptr; return *this; }
		Wg_ObjRef(const Wg_ObjRef&) = delete;
		Wg_ObjRef& operator=(const Wg_ObjRef&) = delete;
		~Wg_ObjRef() { if (obj) Wg_DecRef(obj); }
		Wg_Obj* Get() const { return obj; }
	private:
		Wg_Obj* obj;
	};

	struct Builtins {
		// Types
		Wg_Obj* object;
		Wg_Obj* noneType;
		Wg_Obj* _bool;
		Wg_Obj* _int;
		Wg_Obj* _float;
		Wg_Obj* str;
		Wg_Obj* tuple;
		Wg_Obj* list;
		Wg_Obj* dict;
		Wg_Obj* set;
		Wg_Obj* func;
		Wg_Obj* slice;
		Wg_Obj* defaultIter;
		Wg_Obj* defaultReverseIter;
		Wg_Obj* dictKeysIter;
		Wg_Obj* dictValuesIter;
		Wg_Obj* dictItemsIter;
		Wg_Obj* setIter;
		Wg_Obj* codeObject;
		Wg_Obj* moduleObject;
		Wg_Obj* file;
		Wg_Obj* readlineIter;

		// Exception types
		Wg_Obj* baseException;
		Wg_Obj* systemExit;
		Wg_Obj* exception;
		Wg_Obj* stopIteration;
		Wg_Obj* arithmeticError;
		Wg_Obj* overflowError;
		Wg_Obj* zeroDivisionError;
		Wg_Obj* attributeError;
		Wg_Obj* importError;
		Wg_Obj* syntaxError;
		Wg_Obj* lookupError;
		Wg_Obj* indexError;
		Wg_Obj* keyError;
		Wg_Obj* memoryError;
		Wg_Obj* nameError;
		Wg_Obj* osError;
		Wg_Obj* isADirectoryError;
		Wg_Obj* runtimeError;
		Wg_Obj* notImplementedError;
		Wg_Obj* recursionError;
		Wg_Obj* typeError;
		Wg_Obj* valueError;

		// Functions
		Wg_Obj* isinstance;
		Wg_Obj* repr;
		Wg_Obj* hash;
		Wg_Obj* len;

		// Instances
		Wg_Obj* none;
		Wg_Obj* _true;
		Wg_Obj* _false;
		Wg_Obj* memoryErrorInstance;
		Wg_Obj* recursionErrorInstance;

		auto GetAll() const {
			return std::array{
				object, noneType, _bool, _int, _float, str, tuple, list,
				dict, set, func, slice, defaultIter, defaultReverseIter,
				dictKeysIter, dictValuesIter, dictItemsIter, setIter,
				codeObject, moduleObject, file, readlineIter,

				baseException, systemExit, exception, stopIteration, arithmeticError,
				overflowError, zeroDivisionError, attributeError, importError,
				syntaxError, lookupError, indexError, keyError, memoryError,
				osError, isADirectoryError, nameError, runtimeError, notImplementedError, recursionError,
				typeError, valueError,

				isinstance, repr, hash, len,

				none, _true, _false, memoryErrorInstance, recursionError,
			};
		}
	};
	
	constexpr const char* DEFAULT_FUNC_NAME = "<unnamed>";
}

struct Wg_Obj {
	struct Func {
		Wg_Obj* self;
		Wg_Function fptr;
		void* userdata;
		bool isMethod;
		std::string module;
		std::string prettyName;
	};

	struct Class {
		std::string name;
		std::string module;
		Wg_Function ctor;
		void* userdata;
		std::vector<Wg_Obj*> bases;
		wings::AttributeTable instanceAttributes;
	};

	std::string type;
	union {
		void* data;

		// For debugging only
		bool* _bool;
		Wg_int* _int;
		Wg_float* _float;
		std::string* _str;
		std::vector<Wg_Obj*>* _list;
		wings::WDict* _map;
		wings::WSet* _set;
		Func* _func;
		Class* klass;
	};
	template <class T> const T& Get() const { return *(const T*)data; }
	template <class T> T& Get() { return *(T*)data; }

	wings::AttributeTable attributes;
	std::vector<std::pair<Wg_Finalizer, void*>> finalizers;
	Wg_Context* context;
	uint32_t refCount = 0;
};

struct Wg_Context {
	Wg_Config config{};
	wings::Rng rng;
	bool closing = false;
	bool gcRunning = false;
	
	// Garbage collection
	size_t lastObjectCountAfterGC = 0;
	std::deque<std::unique_ptr<Wg_Obj>> mem;
	std::vector<wings::Executor*> executors;

	// Object instances
	using Globals = std::unordered_map<std::string, wings::RcPtr<Wg_Obj*>>;
	std::unordered_map<std::string, Globals> globals;
	wings::Builtins builtins{};
	Wg_Obj* argv = nullptr;
	
	// Exception info
	std::vector<wings::TraceFrame> currentTrace;
	std::vector<wings::OwnedTraceFrame> exceptionTrace;
	std::string traceMessage;
	Wg_Obj* currentException = nullptr;
	
	// Function call data
	std::vector<Wg_Obj*> kwargs;
	std::vector<void*> userdata;
	std::vector<Wg_Obj*> reprStack;

	// Imports
	std::unordered_map<std::string, Wg_ModuleLoader> moduleLoaders;
	std::stack<std::string_view> currentModule;
	std::string importPath;
};

#define WG_UNREACHABLE() std::abort()

#define WG_STRINGIZE_HELPER(x) WG_STRINGIZE2_HELPER(x)
#define WG_STRINGIZE2_HELPER(x) #x
#define WG_LINE_AS_STRING WG_STRINGIZE_HELPER(__LINE__)

#ifndef WG_NO_ASSERT
	#define WG_ASSERT_RET(ret, assertion) do { if (!(assertion)) { wings::CallErrorCallback( \
	WG_LINE_AS_STRING " " __FILE__ " " #assertion \
	); return ret; } } while (0)
#else
	#define WG_ASSERT_RET(ret, assertion)
#endif

#define WG_ASSERT(assertion) WG_ASSERT_RET({}, assertion)
#define WG_ASSERT_VOID(assertion) WG_ASSERT_RET(void(), assertion)

#define WG_EXPECT_ARG_COUNT(n) do if (argc != n) { Wg_RaiseArgumentCountError(context, argc, n); return nullptr; } while (0)
#define WG_EXPECT_ARG_COUNT_AT_LEAST(n) do if (argc < n) { Wg_RaiseArgumentCountError(context, argc, n); return nullptr; } while (0)
#define WG_EXPECT_ARG_COUNT_BETWEEN(min, max) do if (argc < min || argc > max) { Wg_RaiseArgumentCountError(context, argc, -1); return nullptr; } while (0)
#define WG_EXPECT_ARG_TYPE(index, check, expect) do if (!(check)(argv[index])) { Wg_RaiseArgumentTypeError(context, index, expect); return nullptr; } while (0)
#define WG_EXPECT_ARG_TYPE_NULL(index) WG_EXPECT_ARG_TYPE(index, Wg_IsNone, "NoneType")
#define WG_EXPECT_ARG_TYPE_BOOL(index) WG_EXPECT_ARG_TYPE(index, Wg_IsBool, "bool")
#define WG_EXPECT_ARG_TYPE_INT(index) WG_EXPECT_ARG_TYPE(index, Wg_IsInt, "int")
#define WG_EXPECT_ARG_TYPE_FLOAT(index) WG_EXPECT_ARG_TYPE(index, [](const Wg_Obj* v) { return Wg_IsIntOrFloat(v) && !Wg_IsInt(v); }, "int or float")
#define WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(index) WG_EXPECT_ARG_TYPE(index, Wg_IsIntOrFloat, "int or float")
#define WG_EXPECT_ARG_TYPE_STRING(index) WG_EXPECT_ARG_TYPE(index, Wg_IsString, "str")
#define WG_EXPECT_ARG_TYPE_LIST(index) WG_EXPECT_ARG_TYPE(index, Wg_IsList, "list")
#define WG_EXPECT_ARG_TYPE_TUPLE(index) WG_EXPECT_ARG_TYPE(index, Wg_IsTuple, "tuple")
#define WG_EXPECT_ARG_TYPE_MAP(index) WG_EXPECT_ARG_TYPE(index, Wg_IsDictionary, "dict")
#define WG_EXPECT_ARG_TYPE_SET(index) WG_EXPECT_ARG_TYPE(index, Wg_IsSet, "set")
#define WG_EXPECT_ARG_TYPE_FUNC(index) WG_EXPECT_ARG_TYPE(index, Wg_IsFunction, "function")


#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <array>
#include <unordered_set>
#include <queue>
#include <optional>
#include <cmath>
#include <algorithm>
#include <bit>
#include <cstring>

namespace wings {
	static const char* const BUILTINS_CODE = R"(
class __DefaultIter:
	def __init__(self, iterable):
		self.iterable = iterable
		self.i = 0
	def __next__(self):
		try:
			val = self.iterable[self.i]
		except IndexError:
			raise StopIteration
		self.i += 1
		return val
	def __iter__(self):
		return self

class __DefaultReverseIter:
	def __init__(self, iterable):
		self.iterable = iterable
		self.i = len(iterable) - 1
	def __next__(self):
		if self.i >= 0:
			val = self.iterable[self.i]
			self.i -= 1
			return val
		raise StopIteration
	def __iter__(self):
		return self

class __RangeIter:
	def __init__(self, start, stop, step):
		self.cur = start
		self.stop = stop
		self.step = step
	def __next__(self):
		cur = self.cur
		if self.step > 0:
			if cur >= self.stop:
				raise StopIteration
		else:
			if cur <= self.stop:
				raise StopIteration
		self.cur = cur + self.step
		return cur
	def __iter__(self):
		return self

class __CodeObject:
	def __init__(self, f):
		self.f = f

class __ReadLineIter:
	def __init__(self, f):
		self.f = f
	def __next__(self):
		line = self.f.readline()
		if line == "":
			raise StopIteration
		return line
	def __iter__(self):
		return self

def abs(x):
	return x.__abs__()

def all(x):
	for v in x:
		if not v:
			return False
	return True

def any(x):
	for v in x:
		if v:
			return True
	return False

def divmod(a, b):
	return (a // b, a % b)

class enumerate:
	def __init__(self, x, start=0):
		self.iter = iter(x)
		self.i = start
	def __iter__(self):
		return self
	def __next__(self):
		i = self.i
		self.i += 1
		return (i, next(self.iter))

class filter:
	def __init__(self, f, iterable):
		self.f = f
		self.iter = iter(iterable)
	def __iter__(self):
		return self
	def __next__(self):
		while True:
			val = next(self.iter)
			if self.f(val):
				return val
		raise StopIteration

def hasattr(obj, name):
	try:
		getattr(obj, name)
		return True
	except AttributeError:
		return False

def hash(x):
	v = x.__hash__()
	if not isinstance(v, int):
		raise TypeError("__hash__() returned a non integer type")
	return v

def iter(x):
	return x.__iter__()

def len(x):
	v = x.__len__()
	if not isinstance(v, int):
		raise TypeError("__len__() returned a non integer type")
	elif v < 0:
		raise ValueError("__len__() returned a negative value")
	return v

class map:
	def __init__(self, f, iterable):
		self.f = f
		self.iter = iter(iterable)
	def __iter__(self):
		return self
	def __next__(self):
		return self.f(next(self.iter))

def max(*args, **kwargs):
	if len(args) == 1:
		args = list(args[0])
	else:
		args = list(args)

	if len(args) == 0:
		if "default" in kwargs:
			return kwargs["default"]
		raise ValueError("max() arg is an empty sequence")
		
	if "key" in kwargs:
		key = kwargs["key"]
	else:
		key = lambda x: x
	
	m = args[0]
	for i in range(1, len(args)):
		if key(args[i]) > key(m):
			m = args[i]
	return m

def min(*args, **kwargs):
	if len(args) == 1:
		args = list(args[0])
	else:
		args = list(args)

	if len(args) == 0:
		if "default" in kwargs:
			return kwargs["default"]
		raise ValueError("min() arg is an empty sequence")
		
	if "key" in kwargs:
		key = kwargs["key"]
	else:
		key = lambda x: x
	
	m = args[0]
	for i in range(1, len(args)):
		if key(args[i]) < key(m):
			m = args[i]
	return m

def next(x):
	return x.__next__()

def pow(x, y):
	return x ** y

class range:
	def __init__(self, start, stop=None, step=None):
		if step is 0:
			raise ValueError("step cannot be 0")
		if stop == None:
			if not isinstance(start, int):
				raise TypeError("stop must be an integer")
			self.start = 0
			self.stop = start
			self.step = 1
		elif step is None:
			if not isinstance(start, int):
				raise TypeError("start must be an integer")
			elif not isinstance(stop, int):
				raise TypeError("start must be an integer")
			self.start = start
			self.stop = stop
			self.step = 1
		else:
			if not isinstance(start, int):
				raise TypeError("start must be an integer")
			elif not isinstance(stop, int):
				raise TypeError("start must be an integer")
			elif not isinstance(step, int):
				raise TypeError("step must be an integer")
			self.start = start
			self.stop = stop
			self.step = step
	def __iter__(self):
		return __RangeIter(self.start, self.stop, self.step)
	def __reversed__(self):
		return range(self.stop - self.step, self.start - self.step, -self.step)

def repr(x):
	v = x.__repr__()
	if not isinstance(v, str):
		raise TypeError("__repr__() returned a non string type")
	return v

def reversed(x):
	return x.__reversed__()

class slice:
	def __init__(self, start, stop=None, step=None):
		if stop is None and step is None:
			self.start = None
			self.stop = start
			self.step = None
		elif step is None:
			self.start = start
			self.stop = stop
			self.step = None
		else:
			self.start = start
			self.stop = stop
			self.step = step
	def __index__(self):
		return self

def sorted(iterable, key=None, reverse=False):
	li = list(iterable)
	li.sort(key=key, reverse=reverse)
	return li

def sum(iterable, start=0):
	n = start
	for i in iterable:
		n += i
	return n

def type(x):
	return x.__class__

class zip:
	def __init__(self, *iterables):
		self.iters = [iter(i) for i in iterables]
	def __iter__(self):
		return self
	def __next__(self):
		return tuple([next(i) for i in self.iters])

class BaseException:
	def __init__(self, message=""):
		self._message = message
	def __str__(self):
		return self._message

class SystemExit(BaseException):
	pass

class Exception(BaseException):
	pass

class StopIteration(Exception):
	pass

class ArithmeticError(Exception):
	pass

class OverflowError(ArithmeticError):
	pass

class ZeroDivisionError(ArithmeticError):
	pass

class AttributeError(Exception):
	pass

class ImportError(Exception):
	pass

class LookupError(Exception):
	pass

class IndexError(LookupError):
	pass

class KeyError(LookupError):
	pass

class MemoryError(Exception):
	pass

class NameError(Exception):
	pass

class OSError(Exception):
	pass

class IsADirectoryError(OSError):
	pass

class RuntimeError(Exception):
	pass

class NotImplementedError(RuntimeError):
	pass

class RecursionError(RuntimeError):
	pass

class SyntaxError(Exception):
	pass

class TypeError(Exception):
	pass

class ValueError(Exception):
	pass
	)";


	enum class Collection {
		List,
		Tuple,
	};

	static std::string PtrToString(const void* p) {
		std::stringstream ss;
		ss << p;
		return ss.str();
	}

	static bool AbsIndex(Wg_Obj* container, Wg_Obj* index, Wg_int& out, std::optional<Wg_int>& size) {
		Wg_Obj* len = Wg_UnaryOp(WG_UOP_LEN, container);
		if (len == nullptr)
			return false;

		if (!Wg_IsInt(index)) {
			Wg_RaiseException(container->context, WG_EXC_TYPEERROR, "index must be an integer");
			return false;
		}

		Wg_int length = size.has_value() ? size.value() : Wg_GetInt(len);
		Wg_int i = Wg_GetInt(index);

		if (i < 0) {
			out = length + i;
		} else {
			out = i;
		}
		return true;
	}

	static bool AbsIndex(Wg_Obj* container, Wg_Obj* index, Wg_int& out) {
		std::optional<Wg_int> size;
		return AbsIndex(container, index, out, size);
	}

	template <class F>
	static bool IterateRange(Wg_int start, Wg_int stop, Wg_int step, F f) {
		WG_ASSERT(step);
		if (step > 0) {
			for (Wg_int i = (Wg_int)start; i < (Wg_int)stop; i += step)
				if (!f(i))
					return false;
		} else {
			for (Wg_int i = (Wg_int)start; i > (Wg_int)stop; i += step)
				if (!f(i))
					return false;
		}
		return true;
	}

	static bool AbsSlice(Wg_Obj* container, Wg_Obj* slice, Wg_int& start, Wg_int& stop, Wg_int& step) {
		std::optional<Wg_int> size;
		std::vector<Wg_ObjRef> refs;
		refs.emplace_back(container);
		refs.emplace_back(slice);

		Wg_Obj* stepAttr = Wg_GetAttribute(slice, "step");
		refs.emplace_back(stepAttr);
		if (stepAttr == nullptr) {
			return false;
		} else if (Wg_IsNone(stepAttr)) {
			step = 1;
		} else if (!Wg_IsInt(stepAttr)) {
			Wg_RaiseException(slice->context, WG_EXC_TYPEERROR, "slice step attribute must be an integer");
			return false;
		} else if ((step = Wg_GetInt(stepAttr)) == 0) {
			Wg_RaiseException(slice->context, WG_EXC_VALUEERROR, "slice step cannot be 0");
			return false;
		}

		Wg_Obj* startAttr = Wg_GetAttribute(slice, "start");
		refs.emplace_back(startAttr);
		bool hasStart = true;
		if (startAttr == nullptr) {
			return false;
		} else if (Wg_IsNone(startAttr)) {
			hasStart = false;
		} else if (!AbsIndex(container, startAttr, start, size)) {
			return false;
		}

		Wg_Obj* stopAttr = Wg_GetAttribute(slice, "stop");
		refs.emplace_back(stopAttr);
		bool hasStop = true;
		if (stopAttr == nullptr) {
			return false;
		} else if (Wg_IsNone(stopAttr)) {
			hasStop = false;
		} else if (!AbsIndex(container, stopAttr, stop, size)) {
			return false;
		}

		auto getSize = [&](Wg_int& out) {
			if (size.has_value()) {
				out = size.value();
			} else {
				Wg_Obj* len = Wg_UnaryOp(WG_UOP_LEN, container);
				if (len == nullptr)
					return false;
				out = Wg_GetInt(len);
				size = out;
			}
			return true;
		};

		if (!hasStart) {
			if (step < 0) {
				if (!getSize(start))
					return false;
				start--;
			} else {
				start = 0;
			}
		}

		if (!hasStop) {
			if (step < 0) {
				stop = -1;
			} else {
				if (!getSize(stop))
					return false;
			}
		}

		return true;
	}

	static void StringReplace(std::string& str, std::string_view from, std::string_view to, Wg_int count) {
		if (from.empty())
			return;
		size_t start_pos = 0;
		while ((start_pos = str.find(from, start_pos)) != std::string::npos && count > 0) {
			str.replace(start_pos, from.length(), to);
			start_pos += to.length();
			count--;
		}
	}

	static std::vector<std::string> StringSplit(std::string s, std::string_view sep, Wg_int maxSplit) {
		std::vector<std::string> buf;
		size_t pos = 0;
		std::string token;
		while ((pos = s.find(sep)) != std::string::npos && maxSplit > 0) {
			token = s.substr(0, pos);
			if (!token.empty())
				buf.push_back(std::move(token));
			s.erase(0, pos + sep.size());
			maxSplit--;
		}
		if (!s.empty())
			buf.push_back(std::move(s));
		return buf;
	}

	static std::vector<std::string> StringSplitChar(std::string s, std::string_view chars, Wg_int maxSplit) {
		size_t last = 0;
		size_t next = 0;
		std::vector<std::string> buf;
		while ((next = s.find_first_of(chars, last)) != std::string::npos && maxSplit > 0) {
			if (next > last)
				buf.push_back(s.substr(last, next - last));
			last = next + 1;
			maxSplit--;
		}
		if (last < s.size())
			buf.push_back(s.substr(last));
		return buf;
	}

	static std::vector<std::string> StringSplitLines(std::string s) {
		size_t last = 0;
		size_t next = 0;
		std::vector<std::string> buf;
		while ((next = s.find_first_of("\r\n", last)) != std::string::npos) {
			buf.push_back(s.substr(last, next - last));
			last = next + 1;
			if (s[next] == '\r' && next + 1 < s.size() && s[next + 1] == '\n')
				last++;
		}
		if (last < s.size())
			buf.push_back(s.substr(last));
		return buf;
	}

	static bool IsSpace(char c) {
		return c == ' ' || c == '\t' || c == '\n' || c == '\r'
			|| c == '\v' || c == '\f';
	}

	static bool MergeSort(Wg_Obj** data, size_t len, Wg_Obj* key) {
		if (len == 1)
			return true;

		Wg_Obj** left = data;
		size_t leftSize = len / 2;
		Wg_Obj** right = data + leftSize;
		size_t rightSize = len - leftSize;
		if (!MergeSort(left, leftSize, key))
			return false;
		if (!MergeSort(right, rightSize, key))
			return false;

		std::vector<Wg_Obj*> buf(len);
		size_t a = 0;
		size_t b = 0;
		for (size_t i = 0; i < len; i++) {
			if (a == leftSize) {
				// No more elements on the left
				buf[i] = right[b];
				b++;
			} else if (b == rightSize) {
				// No more elements on the right
				buf[i] = left[a];
				a++;
			} else {
				Wg_Obj* leftMapped = (key && !Wg_IsNone(key)) ? Wg_Call(key, &left[a], 1) : left[a];
				if (leftMapped == nullptr)
					return false;
				Wg_Obj* rightMapped = (key && !Wg_IsNone(key)) ? Wg_Call(key, &right[b], 1) : right[b];
				if (rightMapped == nullptr)
					return false;

				Wg_Obj* gt = Wg_BinaryOp(WG_BOP_LE, rightMapped, leftMapped);
				if (gt == nullptr)
					return false;

				if (Wg_GetBool(gt)) {
					// right < left
					buf[i] = right[b];
					b++;
				} else {
					// right >= left
					buf[i] = left[a];
					a++;
				}
			}
		}
	
		for (size_t i = 0; i < len; i++)
			data[i] = buf[i];
		return true;
	}
	
	namespace ctors {

		static Wg_Obj* object(Wg_Context* context, Wg_Obj**, int argc) { // Excludes self
			WG_EXPECT_ARG_COUNT(0);

			Wg_Obj* obj = Alloc(context);
			if (obj == nullptr)
				return nullptr;
			
			obj->attributes = context->builtins.object->Get<Wg_Obj::Class>().instanceAttributes.Copy();
			obj->type = "__object";
			return obj;
		}

		static Wg_Obj* none(Wg_Context* context, Wg_Obj**, int) { // Excludes self
			return context->builtins.none;
		}

		static Wg_Obj* _bool(Wg_Context* context, Wg_Obj** argv, int argc) { // Excludes self
			WG_EXPECT_ARG_COUNT_BETWEEN(0, 1);

			if (argc == 1) {
				Wg_Obj* res = Wg_CallMethod(argv[0], "__nonzero__", nullptr, 0);
				if (res == nullptr) {
					return nullptr;
				} else if (!Wg_IsBool(res)) {
					Wg_RaiseException(context, WG_EXC_TYPEERROR, "__nonzero__() returned a non bool type");
					return nullptr;
				}
				return res;
			}

			return context->builtins._false;
		}
		
		static Wg_Obj* _int(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 3);

			Wg_int v = 0;
			if (argc >= 2) {
				Wg_Obj* res = Wg_CallMethod(argv[1], "__int__", argv + 2, argc - 2);
				if (res == nullptr) {
					return nullptr;
				} else if (!Wg_IsInt(res)) {
					Wg_RaiseException(context, WG_EXC_TYPEERROR, "__int__() returned a non int type");
					return nullptr;
				}
				v = Wg_GetInt(res);
			}

			argv[0]->attributes = context->builtins._int->Get<Wg_Obj::Class>().instanceAttributes.Copy();
			argv[0]->type = "__int";
			
			auto data = new Wg_int(v);
			Wg_SetUserdata(argv[0], data);
			Wg_RegisterFinalizer(argv[0], [](void* ud) { delete (Wg_int*)ud; }, data);

			return Wg_None(context);
		}

		static Wg_Obj* _float(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 2);

			Wg_float v = 0;
			if (argc == 2) {
				Wg_Obj* res = Wg_CallMethod(argv[1], "__float__", nullptr, 0);
				if (res == nullptr) {
					return nullptr;
				} else if (!Wg_IsIntOrFloat(res)) {
					Wg_RaiseException(context, WG_EXC_TYPEERROR, "__float__() returned a non float type");
					return nullptr;
				}
				v = Wg_GetFloat(res);
			}

			argv[0]->attributes = context->builtins._float->Get<Wg_Obj::Class>().instanceAttributes.Copy();
			argv[0]->type = "__float";

			auto data = new Wg_float(v);
			Wg_SetUserdata(argv[0], data);
			Wg_RegisterFinalizer(argv[0], [](void* ud) { delete (Wg_float*)ud; }, data);

			return Wg_None(context);
		}

		static Wg_Obj* str(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 2);

			const char* v = "";
			if (argc == 2) {
				Wg_Obj* res = Wg_CallMethod(argv[1], "__str__", nullptr, 0);
				if (res == nullptr) {
					return nullptr;
				} else if (!Wg_IsString(res)) {
					Wg_RaiseException(context, WG_EXC_TYPEERROR, "__str__() returned a non string type");
					return nullptr;
				}
				v = Wg_GetString(res);
			}
			argv[0]->attributes = context->builtins.str->Get<Wg_Obj::Class>().instanceAttributes.Copy();
			argv[0]->type = "__str";

			auto data = new std::string(v);
			Wg_SetUserdata(argv[0], data);
			Wg_RegisterFinalizer(argv[0], [](void* ud) { delete (std::string*)ud; }, data);

			return Wg_None(context);
		}

		static Wg_Obj* tuple(Wg_Context* context, Wg_Obj** argv, int argc) { // Excludes self
			WG_EXPECT_ARG_COUNT_BETWEEN(0, 1);

			struct State {
				std::vector<Wg_Obj*> v;
				std::vector<Wg_ObjRef> refs;
			} s;
			if (argc == 1) {
				auto f = [](Wg_Obj* x, void* u) {
					State* s = (State*)u;
					s->refs.emplace_back(x);
					s->v.push_back(x);
					return true;
				};

				if (!Wg_Iterate(argv[0], &s, f))
					return nullptr;
			}

			Wg_Obj* obj = Alloc(context);
			if (obj == nullptr)
				return nullptr;
			
			obj->attributes = context->builtins.tuple->Get<Wg_Obj::Class>().instanceAttributes.Copy();
			obj->type = "__tuple";

			auto data = new std::vector<Wg_Obj*>(std::move(s.v));
			Wg_SetUserdata(obj, data);
			Wg_RegisterFinalizer(obj, [](void* ud) { delete (std::vector<Wg_Obj*>*)ud; }, data);

			return obj;
		}

		static Wg_Obj* list(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 2);

			struct State {
				std::vector<Wg_Obj*> v;
				std::vector<Wg_ObjRef> refs;
			} s;
			if (argc == 2) {
				auto f = [](Wg_Obj* x, void* u) {
					State* s = (State*)u;
					s->refs.emplace_back(x);
					s->v.push_back(x);
					return true;
				};

				if (!Wg_Iterate(argv[1], &s, f))
					return nullptr;
			}

			argv[0]->attributes = context->builtins.list->Get<Wg_Obj::Class>().instanceAttributes.Copy();
			argv[0]->type = "__list";

			auto data = new std::vector<Wg_Obj*>(std::move(s.v));
			Wg_SetUserdata(argv[0], data);
			Wg_RegisterFinalizer(argv[0], [](void* ud) { delete (std::vector<Wg_Obj*>*)ud; }, data);

			return Wg_None(context);
		}

		static Wg_Obj* map(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 2);
			
			argv[0]->attributes = context->builtins.dict->Get<Wg_Obj::Class>().instanceAttributes.Copy();
			argv[0]->type = "__map";

			auto data = new WDict();
			Wg_SetUserdata(argv[0], data);
			Wg_RegisterFinalizer(argv[0], [](void* ud) { delete (WDict*)ud; }, data);

			if (argc == 2) {
				Wg_Obj* iterable = argv[1];
				if (Wg_IsDictionary(argv[1])) {
					iterable = Wg_CallMethod(argv[1], "items", nullptr, 0);
				}

				auto f = [](Wg_Obj* obj, void* ud) {
					Wg_Obj* kv[2]{};
					if (!Wg_Unpack(obj, 2, kv))
						return false;

					Wg_ObjRef ref(kv[1]);
					try {
						((WDict*)ud)->operator[](kv[0]) = kv[1];
					} catch (HashException&) {}
					return true;
				};

				if (!Wg_Iterate(iterable, data, f))
					return nullptr;
			}

			for (const auto& [k, v] : Wg_GetKwargs(context)->Get<WDict>()) {
				try {
					data->operator[](k) = v;
				} catch (HashException&) {
					return nullptr;
				}
			}

			return Wg_None(context);
		}

		static Wg_Obj* set(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 2);
			
			argv[0]->attributes = context->builtins.set->Get<Wg_Obj::Class>().instanceAttributes.Copy();
			argv[0]->type = "__set";

			auto data = new WSet();
			Wg_SetUserdata(argv[0], data);
			Wg_RegisterFinalizer(argv[0], [](void* ud) { delete (WSet*)ud; }, data);

			if (argc == 2) {
				Wg_Obj* iterable = argv[1];
				auto f = [](Wg_Obj* obj, void* ud) {
					try {
						((WSet*)ud)->insert(obj);
					} catch (HashException&) {}
					return true;
				};

				if (!Wg_Iterate(iterable, data, f))
					return nullptr;
			}

			return Wg_None(context);
		}

		static Wg_Obj* BaseException(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 2);
			if (argc == 2) {
				Wg_SetAttribute(argv[0], "_message", argv[1]);
				return Wg_None(context);
			} else if (Wg_Obj* msg = Wg_NewString(context)) {
				Wg_SetAttribute(argv[0], "_message", msg);
				return Wg_None(context);
			} else {
				return nullptr;
			}
		}

		static Wg_Obj* DictIter(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_MAP(1);
			auto* it = new WDict::iterator(argv[1]->Get<WDict>().begin());
			Wg_SetUserdata(argv[0], it);
			Wg_RegisterFinalizer(argv[0], [](void* ud) { delete (WDict::iterator*)ud; }, it);

			Wg_IncRef(argv[1]);
			Wg_RegisterFinalizer(argv[0], [](void* ud) { Wg_DecRef((Wg_Obj*)ud); }, argv[1]);
			return Wg_None(context);
		}

		static Wg_Obj* SetIter(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_SET(1);
			auto* it = new WSet::iterator(argv[1]->Get<WSet>().begin());
			Wg_SetUserdata(argv[0], it);
			Wg_RegisterFinalizer(argv[0], [](void* ud) { delete (WSet::iterator*)ud; }, it);

			Wg_IncRef(argv[1]);
			Wg_RegisterFinalizer(argv[0], [](void* ud) { Wg_DecRef((Wg_Obj*)ud); }, argv[1]);
			return Wg_None(context);
		}

		static Wg_Obj* File(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(2, 3);
			WG_EXPECT_ARG_TYPE_STRING(1);

			const char* filename = Wg_GetString(argv[1]);

			std::ios::openmode mode{};
			if (argc == 3) {
				WG_EXPECT_ARG_TYPE_STRING(2);
				std::string m = Wg_GetString(argv[2]);
	
				size_t b;
				if ((b = m.find('b')) != std::string::npos) {
					mode |= std::ios::binary;
					m.erase(b);
				}
				
				if (m == "r") {
					mode = std::ios::in;
				} else if (m == "w") {
					mode = std::ios::out;
				} else if (m == "a") {
					mode = std::ios::app;
				} else if (m == "r+") {
					mode = std::ios::in | std::ios::out;
				} else if (m == "w+") {
					mode = std::ios::in | std::ios::out | std::ios::trunc;
				} else if (m == "a+") {
					mode = std::ios::in | std::ios::app;
				} else {
					Wg_RaiseException(context, WG_EXC_VALUEERROR, "Invalid file mode");
					return nullptr;
				}
			} else {
				mode = std::ios::in;
			}

			auto* f = new std::fstream(filename, mode);
			if (!f->is_open()) {
				Wg_RaiseException(context, WG_EXC_OSERROR, "Failed to open file");
				return nullptr;
			}

			Wg_SetUserdata(argv[0], f);
			Wg_RegisterFinalizer(argv[0], [](void* ud) { delete (std::fstream*)ud; }, f);

			bool readable = mode & std::ios::in;
			bool writable = mode & std::ios::out;
			
			Wg_SetAttribute(argv[0], "_readable", Wg_NewBool(context, readable));
			Wg_SetAttribute(argv[0], "_writable", Wg_NewBool(context, writable));
			
			return Wg_None(context);
		}
		
	} // namespace ctors

	namespace methods {
		
		static Wg_Obj* object_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			if (Wg_IsClass(argv[0])) {
				std::string s = "<class '" + argv[0]->Get<Wg_Obj::Class>().name + "'>";
				return Wg_NewString(context, s.c_str());
			} else {
				std::string s = "<" + WObjTypeToString(argv[0]) + " object at 0x" + PtrToString(argv[0]) + ">";
				return Wg_NewString(context, s.c_str());
			}
		}

		static Wg_Obj* object_nonzero(Wg_Context* context, Wg_Obj**, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			return Wg_NewBool(context, true);
		}

		static Wg_Obj* object_repr(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			return Wg_UnaryOp(WG_UOP_STR, argv[0]);
		}

		static Wg_Obj* object_eq(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			return Wg_NewBool(context, argv[0] == argv[1]);
		}

		static Wg_Obj* object_ne(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			Wg_Obj* eq = Wg_BinaryOp(WG_BOP_EQ, argv[0], argv[1]);
			if (eq == nullptr)
				return nullptr;
			return Wg_NewBool(context, !Wg_GetBool(eq));
		}

		static Wg_Obj* object_le(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			Wg_Obj* lt = Wg_BinaryOp(WG_BOP_LT, argv[0], argv[1]);
			if (lt == nullptr)
				return nullptr;
			if (Wg_GetBool(lt))
				return Wg_NewBool(context, true);			
			return Wg_BinaryOp(WG_BOP_EQ, argv[0], argv[1]);
		}

		static Wg_Obj* object_ge(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			Wg_Obj* lt = Wg_BinaryOp(WG_BOP_LT, argv[0], argv[1]);
			if (lt == nullptr)
				return nullptr;
			return Wg_NewBool(context, !Wg_GetBool(lt));
		}

		static Wg_Obj* object_gt(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			Wg_Obj* lt = Wg_BinaryOp(WG_BOP_LT, argv[0], argv[1]);
			if (lt == nullptr)
				return nullptr;
			if (Wg_GetBool(lt))
				return Wg_NewBool(context, false);

			Wg_Obj* eq = Wg_BinaryOp(WG_BOP_EQ, argv[0], argv[1]);
			if (eq == nullptr)
				return nullptr;
			return Wg_NewBool(context, !Wg_GetBool(eq));
		}

		static Wg_Obj* object_hash(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			Wg_int hash = (Wg_int)std::hash<Wg_Obj*>()(argv[0]);
			return Wg_NewInt(context, hash);
		}

		static Wg_Obj* object_iadd(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__add__", &argv[1], 1);
		}

		static Wg_Obj* object_isub(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__sub__", &argv[1], 1);
		}

		static Wg_Obj* object_imul(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__mul__", &argv[1], 1);
		}

		static Wg_Obj* object_itruediv(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__truediv__", &argv[1], 1);
		}

		static Wg_Obj* object_ifloordiv(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__floordiv__", &argv[1], 1);
		}

		static Wg_Obj* object_imod(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__mod__", &argv[1], 1);
		}

		static Wg_Obj* object_ipow(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__pow__", &argv[1], 1);
		}

		static Wg_Obj* object_iand(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__and__", &argv[1], 1);
		}

		static Wg_Obj* object_ior(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__or__", &argv[1], 1);
		}

		static Wg_Obj* object_ixor(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__xor__", &argv[1], 1);
		}

		static Wg_Obj* object_ilshift(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__lshift__", &argv[1], 1);
		}

		static Wg_Obj* object_irshift(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__rshift__", &argv[1], 1);
		}

		static Wg_Obj* object_iter(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			return Wg_Call(context->builtins.defaultIter, argv, 1);
		}

		static Wg_Obj* object_reversed(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			return Wg_Call(context->builtins.defaultReverseIter, argv, 1);
		}

		static Wg_Obj* null_nonzero(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_NULL(0);
			return Wg_NewBool(context, false);
		}

		static Wg_Obj* null_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_NULL(0);
			return Wg_NewString(context, "None");
		}

		static Wg_Obj* bool_int(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_BOOL(0);
			return Wg_NewInt(context, Wg_GetBool(argv[0]) ? 1 : 0);
		}

		static Wg_Obj* bool_float(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_BOOL(0);
			return Wg_NewFloat(context, Wg_GetBool(argv[0]) ? (Wg_float)1 : (Wg_float)0);
		}

		static Wg_Obj* bool_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_BOOL(0);
			return Wg_NewString(context, Wg_GetBool(argv[0]) ? "True" : "False");
		}

		static Wg_Obj* bool_eq(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_BOOL(0);
			return Wg_NewBool(context, Wg_IsBool(argv[1]) && Wg_GetBool(argv[0]) == Wg_GetBool(argv[1]));
		}

		static Wg_Obj* bool_hash(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_BOOL(0);
			Wg_int hash = (Wg_int)std::hash<bool>()(Wg_GetBool(argv[0]));
			return Wg_NewInt(context, hash);
		}

		static Wg_Obj* bool_abs(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_BOOL(0);
			return Wg_NewInt(context, Wg_GetBool(argv[0]) ? 1 : 0);
		}

		static Wg_Obj* int_nonzero(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_INT(0);
			return Wg_NewBool(context, Wg_GetInt(argv[0]) != 0);
		}

		static Wg_Obj* int_float(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_INT(0);
			return Wg_NewFloat(context, Wg_GetFloat(argv[0]));
		}

		static Wg_Obj* int_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_INT(0);
			return Wg_NewString(context, std::to_string(argv[0]->Get<Wg_int>()).c_str());
		}

		static Wg_Obj* int_eq(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(0);
			return Wg_NewBool(context, Wg_IsInt(argv[1]) && Wg_GetInt(argv[0]) == Wg_GetInt(argv[1]));
		}

		static Wg_Obj* int_lt(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(0);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return Wg_NewBool(context, Wg_GetFloat(argv[0]) < Wg_GetFloat(argv[1]));
		}

		static Wg_Obj* int_hash(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_INT(0);
			//Wg_int hash = (Wg_int)std::hash<Wg_int>()(Wg_GetInt(argv[0]));
			Wg_int hash = Wg_GetInt(argv[0]);
			return Wg_NewInt(context, hash);
		}

		static Wg_Obj* int_abs(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_INT(0);
			return Wg_NewInt(context, std::abs(Wg_GetInt(argv[0])));
		}

		static Wg_Obj* int_neg(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_INT(0);
			return Wg_NewInt(context, -Wg_GetInt(argv[0]));
		}

		static Wg_Obj* int_add(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(0);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			if (Wg_IsInt(argv[1])) {
				return Wg_NewInt(context, Wg_GetInt(argv[0]) + Wg_GetInt(argv[1]));
			} else {
				return Wg_NewFloat(context, Wg_GetFloat(argv[0]) + Wg_GetFloat(argv[1]));
			}
		}

		static Wg_Obj* int_sub(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(0);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			if (Wg_IsInt(argv[1])) {
				return Wg_NewInt(context, Wg_GetInt(argv[0]) - Wg_GetInt(argv[1]));
			} else {
				return Wg_NewFloat(context, Wg_GetFloat(argv[0]) - Wg_GetFloat(argv[1]));
			}
		}

		static Wg_Obj* int_mul(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(0);

			if (Wg_IsString(argv[1])) {
				Wg_int multiplier = Wg_GetInt(argv[0]);
				std::string s;
				for (Wg_int i = 0; i < multiplier; i++)
					s += Wg_GetString(argv[1]);
				return Wg_NewString(context, s.c_str());
			} else if (Wg_IsInt(argv[1])) {
				return Wg_NewInt(context, Wg_GetInt(argv[0]) * Wg_GetInt(argv[1]));
			} else if (Wg_IsIntOrFloat(argv[1])) {
				return Wg_NewFloat(context, Wg_GetFloat(argv[0]) * Wg_GetFloat(argv[1]));
			} else {
				WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
				return nullptr;
			}
		}

		static Wg_Obj* int_truediv(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(0);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);

			if (Wg_GetFloat(argv[1]) == 0) {
				Wg_RaiseException(context, WG_EXC_ZERODIVISIONERROR);
				return nullptr;
			}
			return Wg_NewFloat(context, Wg_GetFloat(argv[0]) / Wg_GetFloat(argv[1]));
		}

		static Wg_Obj* int_floordiv(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(0);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);

			if (Wg_GetFloat(argv[1]) == 0) {
				Wg_RaiseException(context, WG_EXC_ZERODIVISIONERROR);
				return nullptr;
			}

			if (Wg_IsInt(argv[1])) {
				return Wg_NewInt(context, (Wg_int)std::floor(Wg_GetFloat(argv[0]) / Wg_GetFloat(argv[1])));
			} else {
				return Wg_NewFloat(context, std::floor(Wg_GetFloat(argv[0]) / Wg_GetFloat(argv[1])));
			}
		}

		static Wg_Obj* int_mod(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(0);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);

			if (Wg_GetFloat(argv[1]) == 0) {
				Wg_RaiseException(context, WG_EXC_ZERODIVISIONERROR);
				return nullptr;
			}

			if (Wg_IsInt(argv[1])) {
				Wg_int mod = Wg_GetInt(argv[1]);
				Wg_int m = Wg_GetInt(argv[0]) % mod;
				if (m < 0)
					m += mod;
				return Wg_NewInt(context, m);
			} else {
				return Wg_NewFloat(context, std::fmod(Wg_GetFloat(argv[0]), Wg_GetFloat(argv[1])));
			}
		}

		static Wg_Obj* int_pow(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(0);
			
			if (Wg_IsInt(argv[1])) {
				return Wg_NewInt(context, (Wg_int)std::pow(Wg_GetFloat(argv[0]), Wg_GetFloat(argv[1])));
			} else {
				WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
				return Wg_NewFloat(context, std::pow(Wg_GetFloat(argv[0]), Wg_GetFloat(argv[1])));
			}
		}

		static Wg_Obj* int_and(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(0);
			WG_EXPECT_ARG_TYPE_INT(1);
			return Wg_NewInt(context, Wg_GetInt(argv[0]) & Wg_GetInt(argv[1]));
		}

		static Wg_Obj* int_or(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(0);
			WG_EXPECT_ARG_TYPE_INT(1);
			return Wg_NewInt(context, Wg_GetInt(argv[0]) | Wg_GetInt(argv[1]));
		}

		static Wg_Obj* int_xor(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(0);
			WG_EXPECT_ARG_TYPE_INT(1);
			return Wg_NewInt(context, Wg_GetInt(argv[0]) ^ Wg_GetInt(argv[1]));
		}

		static Wg_Obj* int_invert(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_INT(0);
			return Wg_NewInt(context, ~Wg_GetInt(argv[0]));
		}

		static Wg_Obj* int_lshift(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(0);
			WG_EXPECT_ARG_TYPE_INT(1);

			Wg_int shift = Wg_GetInt(argv[1]);
			if (shift < 0) {
				Wg_RaiseException(context, WG_EXC_VALUEERROR, "Shift cannot be negative");
				return nullptr;
			}
			shift = std::min(shift, (Wg_int)sizeof(Wg_int) * 8);
			return Wg_NewInt(context, Wg_GetInt(argv[0]) << shift);
		}

		static Wg_Obj* int_rshift(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(0);
			WG_EXPECT_ARG_TYPE_INT(1);

			Wg_int shift = Wg_GetInt(argv[1]);
			if (shift < 0) {
				Wg_RaiseException(context, WG_EXC_VALUEERROR, "Shift cannot be negative");
				return nullptr;
			}
			shift = std::min(shift, (Wg_int)sizeof(Wg_int) * 8);
			Wg_uint shifted = (Wg_uint)Wg_GetInt(argv[0]) >> shift;
			Wg_int i{};
			std::memcpy(&i, &shifted, sizeof(Wg_int));
			return Wg_NewInt(context, i);
		}

		static Wg_Obj* int_bit_length(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_INT(0);

			Wg_uint n = (Wg_uint)Wg_GetInt(argv[0]);
			return Wg_NewInt(context, (Wg_int)std::bit_width(n));
		}

		static Wg_Obj* int_bit_count(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_INT(0);

			Wg_uint n = (Wg_uint)Wg_GetInt(argv[0]);
			return Wg_NewInt(context, (Wg_int)std::popcount(n));
		}

		static Wg_Obj* float_nonzero(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			return Wg_NewBool(context, Wg_GetFloat(argv[0]) != 0);
		}

		static Wg_Obj* float_int(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			return Wg_NewInt(context, (Wg_int)Wg_GetFloat(argv[0]));
		}

		static Wg_Obj* float_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_FLOAT(0);
			std::string s = std::to_string(argv[0]->Get<Wg_float>());
			s.erase(s.find_last_not_of('0') + 1, std::string::npos);
			if (s.ends_with('.'))
				s.push_back('0');
			return Wg_NewString(context, s.c_str());
		}

		static Wg_Obj* float_eq(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			return Wg_NewBool(context, Wg_IsIntOrFloat(argv[1]) && Wg_GetFloat(argv[0]) == Wg_GetFloat(argv[1]));
		}

		static Wg_Obj* float_lt(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return Wg_NewBool(context, Wg_GetFloat(argv[0]) < Wg_GetFloat(argv[1]));
		}

		static Wg_Obj* float_hash(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_FLOAT(0);
			Wg_int hash = (Wg_int)std::hash<Wg_float>()(Wg_GetFloat(argv[0]));
			return Wg_NewInt(context, hash);
		}

		static Wg_Obj* float_abs(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_FLOAT(0);
			return Wg_NewFloat(context, std::abs(Wg_GetFloat(argv[0])));
		}

		static Wg_Obj* float_neg(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			return Wg_NewFloat(context, -Wg_GetFloat(argv[0]));
		}

		static Wg_Obj* float_add(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return Wg_NewFloat(context, Wg_GetFloat(argv[0]) + Wg_GetFloat(argv[1]));
		}

		static Wg_Obj* float_sub(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return Wg_NewFloat(context, Wg_GetFloat(argv[0]) - Wg_GetFloat(argv[1]));
		}

		static Wg_Obj* float_mul(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return Wg_NewFloat(context, Wg_GetFloat(argv[0]) * Wg_GetFloat(argv[1]));
		}

		static Wg_Obj* float_truediv(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return Wg_NewFloat(context, Wg_GetFloat(argv[0]) / Wg_GetFloat(argv[1]));
		}

		static Wg_Obj* float_floordiv(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return Wg_NewFloat(context, std::floor(Wg_GetFloat(argv[0]) / Wg_GetFloat(argv[1])));
		}

		static Wg_Obj* float_mod(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return Wg_NewFloat(context, std::fmod(Wg_GetFloat(argv[0]), Wg_GetFloat(argv[1])));
		}

		static Wg_Obj* float_pow(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return Wg_NewFloat(context, std::pow(Wg_GetFloat(argv[0]), Wg_GetFloat(argv[1])));
		}

		static Wg_Obj* float_is_integer(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_FLOAT(0);

			Wg_float f = Wg_GetFloat(argv[0]);
			return Wg_NewBool(context, std::floor(f) == f);
		}

		static Wg_Obj* str_nonzero(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_STRING(0);
			std::string s = Wg_GetString(argv[0]);
			return Wg_NewBool(context, !s.empty());
		}

		static Wg_Obj* str_int(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 2);
			WG_EXPECT_ARG_TYPE_STRING(0);

			constexpr std::string_view DIGITS = "0123456789abcdefghijklmnopqrstuvwxyz";

			auto isDigit = [&](char c, int base = 10) {
				auto sub = DIGITS.substr(0, base);
				return sub.find(std::tolower(c)) != std::string_view::npos;
			};

			auto digitValueOf = [&](char c, int base) {
				return DIGITS.substr(0, base).find(std::tolower(c));
			};

			std::string s = Wg_GetString(argv[0]);
			const char* p = s.c_str();

			std::optional<int> expectedBase;
			if (argc == 2) {
				expectedBase = (int)Wg_GetInt(argv[1]);
			}

			int base = 10;
			if (expectedBase.has_value()) {
				base = expectedBase.value();
			} else if (*p == '0') {
				switch (p[1]) {
				case 'b': case 'B': base = 2; break;
				case 'o': case 'O': base = 8; break;
				case 'x': case 'X': base = 16; break;
				}

				if (base != 10) {
					p += 2;
					if (!isDigit(*p, base)) {
						const char* message{};
						switch (base) {
						case 2: message = "Invalid binary string"; break;
						case 8: message = "Invalid octal string"; break;
						case 16: message = "Invalid hexadecimal string"; break;
						default: WG_UNREACHABLE();
						}
						Wg_RaiseException(context, WG_EXC_VALUEERROR, message);
						return nullptr;
					}
				}
			}

			uintmax_t value = 0;
			for (; *p && isDigit(*p, base); ++p) {
				value = (base * value) + digitValueOf(*p, base);
			}

			if (value > std::numeric_limits<Wg_uint>::max()) {
				Wg_RaiseException(context, WG_EXC_OVERFLOWERROR, "Integer string is too large");
				return nullptr;
			}

			if (*p) {
				Wg_RaiseException(context, WG_EXC_VALUEERROR, "Invalid integer string");
				return nullptr;
			}

			return Wg_NewInt(context, (Wg_int)value);
		}

		static Wg_Obj* str_float(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_STRING(0);

			auto isDigit = [](char c, int base = 10) {
				switch (base) {
				case 2: return c >= '0' && c <= '1';
				case 8: return c >= '0' && c <= '7';
				case 10: return c >= '0' && c <= '9';
				case 16: return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
				default: WG_UNREACHABLE();
				}
			};

			auto digitValueOf = [](char c, int base) {
				switch (base) {
				case 2:
				case 8:
				case 10:
					return c - '0';
				case 16:
					if (c >= '0' && c <= '9') {
						return c - '0';
					} else if (c >= 'a' && c <= 'f') {
						return c - 'a' + 10;
					} else {
						return c - 'A' + 10;
					}
				default:
					WG_UNREACHABLE();
				}
			};

			std::string s = Wg_GetString(argv[0]);
			const char* p = s.c_str();

			if (s == "inf") {
				return Wg_NewFloat(context, std::numeric_limits<Wg_float>::infinity());
			} else if (s == "-inf") {
				return Wg_NewFloat(context, -std::numeric_limits<Wg_float>::infinity());
			} else if (s == "nan") {
				return Wg_NewFloat(context, std::numeric_limits<Wg_float>::quiet_NaN());
			}

			int base = 10;
			if (*p == '0') {
				switch (p[1]) {
				case 'b': case 'B': base = 2; break;
				case 'o': case 'O': base = 8; break;
				case 'x': case 'X': base = 16; break;
				}
			}

			if (base != 10) {
				p += 2;
				if (!isDigit(*p, base) && *p != '.') {
					const char* message{};
					switch (base) {
					case 2: message = "Invalid binary string"; break;
					case 8: message = "Invalid octal string"; break;
					case 16: message = "Invalid hexadecimal string"; break;
					default: WG_UNREACHABLE();
					}
					Wg_RaiseException(context, WG_EXC_VALUEERROR, message);
					return nullptr;
				}
			}

			uintmax_t value = 0;
			for (; *p && isDigit(*p, base); ++p) {
				value = (base * value) + digitValueOf(*p, base);
			}

			Wg_float fvalue = (Wg_float)value;
			if (*p == '.') {
				++p;
				for (int i = 1; *p && isDigit(*p, base); ++p, ++i) {
					fvalue += digitValueOf(*p, base) * std::pow((Wg_float)base, (Wg_float)-i);
				}
			}

			if (*p) {
				Wg_RaiseException(context, WG_EXC_VALUEERROR, "Invalid float string");
				return nullptr;
			}

			return Wg_NewFloat(context, fvalue);
		}

		static Wg_Obj* str_repr(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_STRING(0);
			
			std::string s = "'";
			for (const char* p = Wg_GetString(argv[0]); *p; ++p) {
				if (*p == '\\') {
					s += "\\\\";
				} else if (*p == '\'') {
					s += "\\'";
				} else if (*p == '\n') {
					s += "\\n";
				} else if (*p == '\r') {
					s += "\\r";
				} else if (*p == '\t') {
					s += "\\t";
				} else if (*p == '\b') {
					s += "\\b";
				} else if (*p == '\f') {
					s += "\\f";
				} else if (*p >= 32 && *p <= 126) {
					s.push_back(*p);
				} else {
					s += "\\x";
					s.push_back("0123456789abcdef"[(*p >> 4) & 0xF]);
					s.push_back("0123456789abcdef"[*p & 0xF]);
				}
			}
			s.push_back('\'');
			
			return Wg_NewString(context, s.c_str());
		}

		static Wg_Obj* str_len(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_STRING(0);
			return Wg_NewInt(context, (Wg_int)argv[0]->Get<std::string>().size());
		}

		static Wg_Obj* str_eq(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_STRING(0);
			return Wg_NewBool(context, Wg_IsString(argv[1]) && std::strcmp(Wg_GetString(argv[0]), Wg_GetString(argv[1])) == 0);
		}

		static Wg_Obj* str_lt(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_STRING(0);
			WG_EXPECT_ARG_TYPE_STRING(1);
			return Wg_NewBool(context, std::strcmp(Wg_GetString(argv[0]), Wg_GetString(argv[1])) < 0);
		}

		static Wg_Obj* str_hash(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_STRING(0);
			Wg_int hash = (Wg_int)std::hash<std::string_view>()(Wg_GetString(argv[0]));
			return Wg_NewInt(context, hash);
		}

		static Wg_Obj* str_add(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_STRING(0);
			WG_EXPECT_ARG_TYPE_STRING(1);
			std::string s = Wg_GetString(argv[0]);
			s += Wg_GetString(argv[1]);
			return Wg_NewString(context, s.c_str());
		}

		static Wg_Obj* str_mul(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_STRING(0);
			WG_EXPECT_ARG_TYPE_INT(1);
			Wg_int multiplier = Wg_GetInt(argv[1]);
			std::string_view arg = Wg_GetString(argv[0]);
			std::string s;
			s.reserve(arg.size() * (size_t)multiplier);
			for (Wg_int i = 0; i < multiplier; i++)
				s += arg;
			return Wg_NewString(context, s.c_str());
		}

		static Wg_Obj* str_contains(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_STRING(0);
			WG_EXPECT_ARG_TYPE_STRING(1);
			return Wg_NewBool(context, std::strstr(Wg_GetString(argv[0]), Wg_GetString(argv[1])));
		}

		static Wg_Obj* str_getitem(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_STRING(0);

			if (Wg_IsInstance(argv[1], &context->builtins.slice, 1)) {
				Wg_int start, stop, step;
				if (!AbsSlice(argv[0], argv[1], start, stop, step))
					return nullptr;

				std::string_view s = Wg_GetString(argv[0]);
				std::string sliced;
				bool success = IterateRange(start, stop, step, [&](Wg_int i) {
					if (i >= 0 && i < (Wg_int)s.size())
						sliced.push_back(s[i]);
					return true;
					});

				if (!success)
					return nullptr;

				return Wg_NewString(context, sliced.c_str());
			}

			Wg_Obj* idx = Wg_UnaryOp(WG_UOP_INDEX, argv[1]);
			if (idx == nullptr)
				return nullptr;
			
			if (Wg_IsInt(idx)) {
				Wg_int index;
				if (!AbsIndex(argv[0], idx, index))
					return nullptr;

				std::string_view s = Wg_GetString(argv[0]);
				if (index < 0 || index >= (Wg_int)s.size()) {
					Wg_RaiseException(context, WG_EXC_INDEXERROR);
					return nullptr;
				}

				char buf[2] = { s[index], '\0' };
				return Wg_NewString(context, buf);
			}
			
			Wg_RaiseArgumentTypeError(context, 1, "int or slice");
			return nullptr;
		}

		static Wg_Obj* str_capitalize(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_STRING(0);
			std::string s = Wg_GetString(argv[0]);
			if (!s.empty())
				s[0] = (char)std::toupper(s[0]);
			return Wg_NewString(context, s.c_str());
		}

		static Wg_Obj* str_lower(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_STRING(0);

			std::string s = Wg_GetString(argv[0]);
			std::transform(s.begin(), s.end(), s.begin(),
				[](unsigned char c) { return std::tolower(c); });
			return Wg_NewString(context, s.c_str());
		}

		static Wg_Obj* str_upper(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_STRING(0);

			std::string s = Wg_GetString(argv[0]);
			std::transform(s.begin(), s.end(), s.begin(),
				[](unsigned char c) { return std::toupper(c); });
			return Wg_NewString(context, s.c_str());
		}

		static Wg_Obj* str_casefold(Wg_Context* context, Wg_Obj** argv, int argc) {
			return str_lower(context, argv, argc);
		}

		static Wg_Obj* str_center(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(2, 3);
			WG_EXPECT_ARG_TYPE_STRING(0);
			WG_EXPECT_ARG_TYPE_INT(1);
			if (argc >= 3) WG_EXPECT_ARG_TYPE_STRING(2);
			
			const char* fill = argc == 3 ? Wg_GetString(argv[2]) : " ";
			if (std::strlen(fill) != 1) {
				Wg_RaiseException(context, WG_EXC_TYPEERROR, "The fill character must be exactly one character long");
				return nullptr;
			}

			std::string s = Wg_GetString(argv[0]);
			Wg_int desiredLen = Wg_GetInt(argv[1]);
			while (true) {
				if ((Wg_int)s.size() >= desiredLen)
					break;
				s.push_back(fill[0]);
				if ((Wg_int)s.size() >= desiredLen)
					break;
				s.insert(s.begin(), fill[0]);
			}

			return Wg_NewString(context, s.c_str());
		}

		static Wg_Obj* str_count(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_STRING(0);
			WG_EXPECT_ARG_TYPE_STRING(1);
			
			std::string_view s = Wg_GetString(argv[0]);
			std::string_view search = Wg_GetString(argv[1]);			
			Wg_int count = 0;
			size_t pos = 0;
			while ((pos = s.find(search, pos)) != std::string_view::npos) {
				count++;
				pos += search.size();
			}

			return Wg_NewInt(context, count);
		}

		static Wg_Obj* str_format(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_AT_LEAST(1);
			WG_EXPECT_ARG_TYPE_STRING(0);
			
			const char* fmt = Wg_GetString(argv[0]);
			enum class Mode { Null, Auto, Manual } mode = Mode::Null;
			size_t autoIndex = 0;
			std::string s;
			for (auto p = fmt; *p; ++p) {
				if (*p != '{') {
					s += *p;
					continue;
				}

				size_t index = 0;
				bool useAutoIndexing = true;
				++p;
				while (*p != '}') {
					if (*p >= '0' && *p <= '9') {
						index = 10 * index + ((size_t)*p - '0');
						useAutoIndexing = false;
						++p;
					} else {
						Wg_RaiseException(context, WG_EXC_VALUEERROR, "Invalid format string");
						return nullptr;
					}
				}

				if (useAutoIndexing) {
					if (mode == Mode::Manual) {
						Wg_RaiseException(
							context,
							WG_EXC_VALUEERROR,
							"Cannot switch from manual field numbering to automatic field specification"
						);
						return nullptr;
					}
					mode = Mode::Auto;
					index = autoIndex;
					autoIndex++;
				} else {
					if (mode == Mode::Auto) {
						Wg_RaiseException(
							context,
							WG_EXC_VALUEERROR,
							"Cannot switch from automatic field numbering to manual field specification"
						);
						return nullptr;
					}
					mode = Mode::Manual;
				}

				if ((int)index >= argc - 1) {
					Wg_RaiseException(context, WG_EXC_INDEXERROR);
					return nullptr;
				}

				Wg_Obj* item = Wg_UnaryOp(WG_UOP_STR, argv[index + 1]);
				if (item == nullptr)
					return nullptr;
				s += Wg_GetString(item);
			}

			return Wg_NewString(context, s.c_str());
		}

		static Wg_Obj* str_startswith(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_STRING(0);
			WG_EXPECT_ARG_TYPE_STRING(1);

			std::string_view s = Wg_GetString(argv[0]);
			std::string_view end = Wg_GetString(argv[1]);
			return Wg_NewBool(context, s.starts_with(end));
		}

		static Wg_Obj* str_endswith(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_STRING(0);
			WG_EXPECT_ARG_TYPE_STRING(1);

			std::string_view s = Wg_GetString(argv[0]);
			std::string_view end = Wg_GetString(argv[1]);
			return Wg_NewBool(context, s.ends_with(end));
		}

		template <bool reverse>
		static Wg_Obj* str_findx(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(2, 4);
			WG_EXPECT_ARG_TYPE_STRING(0);
			WG_EXPECT_ARG_TYPE_STRING(1);
			
			Wg_int start = 0;
			std::optional<Wg_int> size;
			if (argc >= 3) {
				WG_EXPECT_ARG_TYPE_INT(2);
				if (!AbsIndex(argv[0], argv[2], start, size))
					return nullptr;
			}

			Wg_int end = 0;
			if (argc >= 4) {
				WG_EXPECT_ARG_TYPE_INT(3);
				if (!AbsIndex(argv[0], argv[3], end, size))
					return nullptr;
			} else {
				Wg_Obj* len = Wg_UnaryOp(WG_UOP_LEN, argv[0]);
				if (len == nullptr)
					return nullptr;
				end = (size_t)Wg_GetInt(len);
			}
			
			std::string_view s = Wg_GetString(argv[0]);
			std::string_view find = Wg_GetString(argv[1]);
			
			Wg_int substrSize = end - start;
			size_t location;
			if (substrSize < 0) {
				location = std::string_view::npos;
			} else {
				start = std::clamp(start, (Wg_int)0, (Wg_int)s.size());
				if (reverse) {
					location = s.substr(start, (size_t)substrSize).rfind(find);
				} else {
					location = s.substr(start, (size_t)substrSize).find(find);
				}
			}
			
			if (location == std::string_view::npos) {
				return Wg_NewInt(context, -1);
			} else {
				return Wg_NewInt(context, (Wg_int)location);
			}
		}

		template <bool reverse>
		static Wg_Obj* str_indexx(Wg_Context* context, Wg_Obj** argv, int argc) {
			Wg_Obj* location = str_findx<reverse>(context, argv, argc);
			if (location == nullptr)
				return nullptr;
			
			if (Wg_GetInt(location) == -1) {
				Wg_RaiseException(context, WG_EXC_VALUEERROR, "substring not found");
				return nullptr;
			} else {
				return location;
			}
		}

		static Wg_Obj* str_find(Wg_Context* context, Wg_Obj** argv, int argc) {
			return str_findx<false>(context, argv, argc);
		}

		static Wg_Obj* str_index(Wg_Context* context, Wg_Obj** argv, int argc) {
			return str_indexx<false>(context, argv, argc);
		}

		static Wg_Obj* str_rfind(Wg_Context* context, Wg_Obj** argv, int argc) {
			return str_findx<true>(context, argv, argc);
		}

		static Wg_Obj* str_rindex(Wg_Context* context, Wg_Obj** argv, int argc) {
			return str_indexx<true>(context, argv, argc);
		}

		template <auto F>
		static Wg_Obj* str_isx(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_STRING(0);

			std::string_view s = Wg_GetString(argv[0]);
			return Wg_NewBool(context, std::all_of(s.begin(), s.end(), F));
		}

		static Wg_Obj* str_isalnum(Wg_Context* context, Wg_Obj** argv, int argc) {
			constexpr auto f = [](char c) { return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9'); };
			return str_isx<f>(context, argv, argc);
		}

		static Wg_Obj* str_isalpha(Wg_Context* context, Wg_Obj** argv, int argc) {
			constexpr auto f = [](char c) { return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z'); };
			return str_isx<f>(context, argv, argc);
		}

		static Wg_Obj* str_isascii(Wg_Context* context, Wg_Obj** argv, int argc) {
			constexpr auto f = [](char c) { return c >= 0 && c < 128; };
			return str_isx<f>(context, argv, argc);
		}

		static Wg_Obj* str_isdigit(Wg_Context* context, Wg_Obj** argv, int argc) {
			constexpr auto f = [](char c) { return '0' <= c && c <= '9'; };
			return str_isx<f>(context, argv, argc);
		}

		static Wg_Obj* str_isdecimal(Wg_Context* context, Wg_Obj** argv, int argc) {
			return str_isdigit(context, argv, argc);
		}

		static Wg_Obj* str_isnumeric(Wg_Context* context, Wg_Obj** argv, int argc) {
			return str_isdigit(context, argv, argc);
		}

		static Wg_Obj* str_isprintable(Wg_Context* context, Wg_Obj** argv, int argc) {
			constexpr auto f = [](char c) { return c >= 32 && c <= 127; };
			return str_isx<f>(context, argv, argc);
		}

		static Wg_Obj* str_isspace(Wg_Context* context, Wg_Obj** argv, int argc) {
			return str_isx<IsSpace>(context, argv, argc);
		}

		static Wg_Obj* str_isupper(Wg_Context* context, Wg_Obj** argv, int argc) {
			constexpr auto f = [](char c) { return !('a' <= c && c <= 'z'); };
			return str_isx<f>(context, argv, argc);
		}

		static Wg_Obj* str_islower(Wg_Context* context, Wg_Obj** argv, int argc) {
			constexpr auto f = [](char c) { return !('A' <= c && c <= 'Z'); };
			return str_isx<f>(context, argv, argc);
		}

		static Wg_Obj* str_isidentifier(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_STRING(0);

			std::string_view s = Wg_GetString(argv[0]);
			constexpr auto f = [](char c) { return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9') || c == '_'; };
			bool allAlphaNum = std::all_of(s.begin(), s.end(), f);
			return Wg_NewBool(context, allAlphaNum && (s.empty() || s[0] < '0' || s[0] > '9'));
		}

		static Wg_Obj* str_join(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_STRING(0);

			struct State {
				std::string_view sep;
				std::string s;
			} state = { Wg_GetString(argv[0]), "" };

			bool success = Wg_Iterate(argv[1], &state, [](Wg_Obj* obj, void* ud) {
				State& state = *(State*)ud;
				Wg_Context* context = obj->context;

				if (!Wg_IsString(obj)) {
					Wg_RaiseException(context, WG_EXC_TYPEERROR, "sequence item must be a string");
					return false;
				}
				
				state.s += Wg_GetString(obj);
				state.s += state.sep;
				return true;
			});

			if (!success)
				return nullptr;

			if (!state.s.empty())
				state.s.erase(state.s.end() - state.sep.size(), state.s.end());

			return Wg_NewString(context, state.s.c_str());
		}

		static Wg_Obj* str_replace(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(3, 4);
			WG_EXPECT_ARG_TYPE_STRING(0);
			WG_EXPECT_ARG_TYPE_STRING(1);
			WG_EXPECT_ARG_TYPE_STRING(2);

			Wg_int count = std::numeric_limits<Wg_int>::max();
			if (argc == 4) {
				WG_EXPECT_ARG_TYPE_INT(3);
				count = Wg_GetInt(argv[3]);
			}

			std::string s = Wg_GetString(argv[0]);
			std::string_view find = Wg_GetString(argv[1]);
			std::string_view repl = Wg_GetString(argv[2]);
			StringReplace(s, find, repl, count);
			return Wg_NewString(context, s.c_str());
		}

		template <bool left, bool zfill = false>
		static Wg_Obj* str_just(Wg_Context* context, Wg_Obj** argv, int argc) {
			if constexpr (zfill) {
				WG_EXPECT_ARG_COUNT(2);
			} else {
				WG_EXPECT_ARG_COUNT_BETWEEN(2, 3);
			}
			WG_EXPECT_ARG_TYPE_STRING(0);
			WG_EXPECT_ARG_TYPE_INT(1);

			char fill = ' ';
			if constexpr (!zfill) {
				if (argc == 3) {
					WG_EXPECT_ARG_TYPE_STRING(0);
					std::string_view fillStr = Wg_GetString(argv[2]);
					if (fillStr.size() != 1) {
						Wg_RaiseException(context, WG_EXC_TYPEERROR, "The fill character must be exactly one character long");
						return nullptr;
					}
					fill = fillStr[0];
				}
			} else {
				fill = '0';
			}

			std::string s = Wg_GetString(argv[0]);

			Wg_int len = Wg_GetInt(argv[1]);
			if (len < (Wg_int)s.size())
				return argv[0];

			if (left) {
				s += std::string((size_t)len - s.size(), fill);
			} else {
				s = s + std::string((size_t)len - s.size(), fill);
			}
			return Wg_NewString(context, s.c_str());
		}

		static Wg_Obj* str_ljust(Wg_Context* context, Wg_Obj** argv, int argc) {
			return str_just<true>(context, argv, argc);
		}

		static Wg_Obj* str_rjust(Wg_Context* context, Wg_Obj** argv, int argc) {
			return str_just<false>(context, argv, argc);
		}

		static Wg_Obj* str_zfill(Wg_Context* context, Wg_Obj** argv, int argc) {
			return str_just<true, true>(context, argv, argc);
		}

		static Wg_Obj* str_lstrip(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 2);
			WG_EXPECT_ARG_TYPE_STRING(0);

			std::string_view chars = " ";
			if (argc == 2 && !Wg_IsNone(argv[1])) {
				WG_EXPECT_ARG_TYPE_STRING(1);
				chars = Wg_GetString(argv[1]);
			}

			std::string_view s = Wg_GetString(argv[0]);
			size_t pos = s.find_first_not_of(chars);
			if (pos == std::string::npos)
				return Wg_NewString(context);
			return Wg_NewString(context, s.data() + pos);
		}

		static Wg_Obj* str_rstrip(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 2);
			WG_EXPECT_ARG_TYPE_STRING(0);

			std::string_view chars = " ";
			if (argc == 2 && !Wg_IsNone(argv[1])) {
				WG_EXPECT_ARG_TYPE_STRING(1);
				chars = Wg_GetString(argv[1]);
			}

			std::string s = Wg_GetString(argv[0]);
			size_t pos = s.find_last_not_of(chars);
			if (pos == std::string::npos)
				return Wg_NewString(context);
			s.erase(s.begin() + pos + 1, s.end());
			return Wg_NewString(context, s.c_str());
		}

		static Wg_Obj* str_strip(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 2);
			WG_EXPECT_ARG_TYPE_STRING(0);

			std::string_view chars = " ";
			if (argc == 2 && !Wg_IsNone(argv[1])) {
				WG_EXPECT_ARG_TYPE_STRING(1);
				chars = Wg_GetString(argv[1]);
			}

			std::string s = Wg_GetString(argv[0]);
			size_t pos = s.find_last_not_of(chars);
			if (pos == std::string::npos)
				return Wg_NewString(context);
			s.erase(s.begin() + pos + 1, s.end());

			pos = s.find_first_not_of(chars);
			if (pos == std::string::npos)
				return Wg_NewString(context);
			return Wg_NewString(context, s.data() + pos);
		}

		static Wg_Obj* str_split(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 3);
			WG_EXPECT_ARG_TYPE_STRING(0);

			Wg_int maxSplit = -1;
			if (argc == 3) {
				WG_EXPECT_ARG_TYPE_INT(2);
				maxSplit = Wg_GetInt(argv[2]);
			}
			if (maxSplit == -1)
				maxSplit = std::numeric_limits<Wg_int>::max();

			std::vector<std::string> strings;
			if (argc >= 2) {
				WG_EXPECT_ARG_TYPE_STRING(1);
				strings = StringSplit(Wg_GetString(argv[0]), Wg_GetString(argv[1]), maxSplit);
			} else {
				strings = StringSplitChar(Wg_GetString(argv[0]), " \t\n\r\v\f", maxSplit);
			}

			Wg_Obj* li = Wg_NewList(context);
			if (li == nullptr)
				return nullptr;
			Wg_ObjRef ref(li);

			for (const auto& s : strings) {
				Wg_Obj* str = Wg_NewString(context, s.c_str());
				if (str == nullptr)
					return nullptr;
				li->Get<std::vector<Wg_Obj*>>().push_back(str);
			}
			return li;
		}

		static Wg_Obj* str_splitlines(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_STRING(0);

			std::vector<std::string> strings = StringSplitLines(Wg_GetString(argv[0]));

			Wg_Obj* li = Wg_NewList(context);
			if (li == nullptr)
				return nullptr;
			Wg_ObjRef ref(li);

			for (const auto& s : strings) {
				Wg_Obj* str = Wg_NewString(context, s.c_str());
				if (str == nullptr)
					return nullptr;
				li->Get<std::vector<Wg_Obj*>>().push_back(str);
			}
			return li;
		}

		template <Collection collection>
		static Wg_Obj* collection_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			constexpr bool isTuple = collection == Collection::Tuple;
			WG_EXPECT_ARG_COUNT(1);
			if constexpr (isTuple) {
				WG_EXPECT_ARG_TYPE_TUPLE(0);
			} else {
				WG_EXPECT_ARG_TYPE_LIST(0);
			}

			auto it = std::find(context->reprStack.rbegin(), context->reprStack.rend(), argv[0]);
			if (it != context->reprStack.rend()) {
				return Wg_NewString(context, isTuple ? "(...)" : "[...]");
			} else {
				context->reprStack.push_back(argv[0]);
				const auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();
				std::string s(1, isTuple ? '(' : '[');
				for (Wg_Obj* child : buf) {
					Wg_Obj* v = Wg_UnaryOp(WG_UOP_REPR, child);
					if (v == nullptr) {
						context->reprStack.pop_back();
						return nullptr;
					}
					s += v->Get<std::string>() + ", ";
				}
				context->reprStack.pop_back();
				if (!buf.empty()) {
					s.pop_back();
					s.pop_back();
				}
				if (isTuple && buf.size() == 1)
					s.push_back(',');
				s += (isTuple ? ')' : ']');
				return Wg_NewString(context, s.c_str());
			}
		}

		template <Collection collection>
		static Wg_Obj* collection_mul(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(1);

			Wg_Obj* col = nullptr;
			if constexpr (collection == Collection::List) {
				WG_EXPECT_ARG_TYPE_LIST(0);
				col = Wg_NewList(context, nullptr, 0);
			} else {
				WG_EXPECT_ARG_TYPE_TUPLE(0);
				col = Wg_NewTuple(context, nullptr, 0);
			}
			if (col == nullptr)
				return nullptr;

			Wg_int mul = Wg_GetInt(argv[1]);
			const auto& thisBuf = argv[0]->Get<std::vector<Wg_Obj*>>();
			auto& buf = col->Get<std::vector<Wg_Obj*>>();
			buf.reserve(mul * thisBuf.size());
			for (Wg_int i = 0; i < mul; i++) {
				buf.insert(buf.end(), thisBuf.begin(), thisBuf.end());
			}
			return col;
		}

		template <Collection collection>
		static Wg_Obj* collection_nonzero(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			if constexpr (collection == Collection::List) {
				WG_EXPECT_ARG_TYPE_LIST(0);
			} else {
				WG_EXPECT_ARG_TYPE_TUPLE(0);
			}

			return Wg_NewBool(context, !argv[0]->Get<std::vector<Wg_Obj*>>().empty());
		}

		template <Collection collection>
		static Wg_Obj* collection_lt(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			if constexpr (collection == Collection::List) {
				WG_EXPECT_ARG_TYPE_LIST(0);
				WG_EXPECT_ARG_TYPE_LIST(1);
			} else {
				WG_EXPECT_ARG_TYPE_TUPLE(0);
				WG_EXPECT_ARG_TYPE_TUPLE(1);
			}

			auto& buf1 = argv[0]->Get<std::vector<Wg_Obj*>>();
			auto& buf2 = argv[1]->Get<std::vector<Wg_Obj*>>();

			size_t minSize = buf1.size() < buf2.size() ? buf1.size() : buf2.size();

			for (size_t i = 0; i < minSize; i++) {
				Wg_Obj* lt = Wg_BinaryOp(WG_BOP_LT, buf1[i], buf2[i]);
				if (lt == nullptr)
					return nullptr;

				if (Wg_GetBool(lt))
					return lt;

				Wg_Obj* gt = Wg_BinaryOp(WG_BOP_LT, buf1[i], buf2[i]);
				if (gt == nullptr)
					return nullptr;

				if (Wg_GetBool(gt))
					return Wg_NewBool(context, false);
			}

			return Wg_NewBool(context, buf1.size() < buf2.size());
		}

		template <Collection collection>
		static Wg_Obj* collection_eq(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			if constexpr (collection == Collection::List) {
				WG_EXPECT_ARG_TYPE_LIST(0);
				if (!Wg_IsInstance(argv[1], &context->builtins.list, 1))
					return Wg_NewBool(context, false);
			} else {
				WG_EXPECT_ARG_TYPE_TUPLE(0);
				if (!Wg_IsInstance(argv[1], &context->builtins.tuple, 1))
					return Wg_NewBool(context, false);
			}

			auto& buf1 = argv[0]->Get<std::vector<Wg_Obj*>>();
			auto& buf2 = argv[1]->Get<std::vector<Wg_Obj*>>();

			if (buf1.size() != buf2.size())
				return Wg_NewBool(context, false);

			for (size_t i = 0; i < buf1.size(); i++) {
				if (Wg_Obj* eq = Wg_BinaryOp(WG_BOP_EQ, buf1[i], buf2[i])) {
					if (!Wg_GetBool(eq))
						return eq;
				} else {
					return nullptr;
				}
			}

			return Wg_NewBool(context, true);
		}

		template <Collection collection>
		static Wg_Obj* collection_contains(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			if constexpr (collection == Collection::List) {
				WG_EXPECT_ARG_TYPE_LIST(0);
			} else {
				WG_EXPECT_ARG_TYPE_TUPLE(0);
			}

			auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();
			for (size_t i = 0; i < buf.size(); i++) {
				if (Wg_Obj* eq = Wg_BinaryOp(WG_BOP_EQ, buf[i], argv[1])) {
					if (Wg_GetBool(eq))
						return eq;
				} else {
					return nullptr;
				}
			}

			return Wg_NewBool(context, false);
		}

		template <Collection collection>
		static Wg_Obj* collection_len(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			if constexpr (collection == Collection::List) {
				WG_EXPECT_ARG_TYPE_LIST(0);
			} else {
				WG_EXPECT_ARG_TYPE_TUPLE(0);
			}

			return Wg_NewInt(context, (Wg_int)argv[0]->Get<std::vector<Wg_Obj*>>().size());
		}

		template <Collection collection>
		static Wg_Obj* collection_count(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			if constexpr (collection == Collection::List) {
				WG_EXPECT_ARG_TYPE_LIST(0);
			} else {
				WG_EXPECT_ARG_TYPE_TUPLE(0);
			}

			auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();
			Wg_int count = 0;
			for (size_t i = 0; i < buf.size(); i++) {
				Wg_Obj* eq = Wg_BinaryOp(WG_BOP_EQ, argv[1], buf[i]);
				if (eq == nullptr)
					return nullptr;
				if (Wg_GetBool(eq))
					count++;
			}

			return Wg_NewInt(context, count);
		}

		template <Collection collection>
		static Wg_Obj* collection_index(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			if constexpr (collection == Collection::List) {
				WG_EXPECT_ARG_TYPE_LIST(0);
			} else {
				WG_EXPECT_ARG_TYPE_TUPLE(0);
			}

			auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();
			for (size_t i = 0; i < buf.size(); i++) {
				Wg_Obj* eq = Wg_BinaryOp(WG_BOP_EQ, argv[1], buf[i]);
				if (eq == nullptr)
					return nullptr;
				if (Wg_GetBool(eq))
					return Wg_NewInt(context, (Wg_int)i);
			}

			Wg_RaiseException(context, WG_EXC_VALUEERROR, "Value was not found");
			return nullptr;
		}

		template <Collection collection>
		static Wg_Obj* collection_getitem(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			if constexpr (collection == Collection::List) {
				WG_EXPECT_ARG_TYPE_LIST(0);
			} else {
				WG_EXPECT_ARG_TYPE_TUPLE(0);
			}

			if (Wg_IsInstance(argv[1], &context->builtins.slice, 1)) {
				Wg_int start, stop, step;
				if (!AbsSlice(argv[0], argv[1], start, stop, step))
					return nullptr;

				auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();
				std::vector<Wg_Obj*> sliced;
				bool success = IterateRange(start, stop, step, [&](Wg_int i) {
					if (i >= 0 && i < (Wg_int)buf.size())
						sliced.push_back(buf[i]);
					return true;
					});

				if (!success)
					return nullptr;

				if constexpr (collection == Collection::List) {
					return Wg_NewList(context, sliced.data(), (int)sliced.size());
				} else {
					return Wg_NewTuple(context, sliced.data(), (int)sliced.size());
				}
			}

			Wg_Obj* idx = Wg_UnaryOp(WG_UOP_INDEX, argv[1]);
			if (idx == nullptr)
				return nullptr;

			if (Wg_IsInt(idx)) {
				Wg_int index;
				if (!AbsIndex(argv[0], idx, index))
					return nullptr;

				auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();
				if (index < 0 || index >= (Wg_int)buf.size()) {
					Wg_RaiseException(context, WG_EXC_INDEXERROR);
					return nullptr;
				}

				return buf[index];
			}

			Wg_RaiseArgumentTypeError(context, 1, "int or slice");
			return nullptr;
		}

		static Wg_Obj* list_setitem(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(3);
			WG_EXPECT_ARG_TYPE_LIST(0);
			WG_EXPECT_ARG_TYPE_INT(1);
			
			Wg_int index;
			if (!AbsIndex(argv[0], argv[1], index))
				return nullptr;

			auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();
			if (index < 0 || index >= (Wg_int)buf.size()) {
				Wg_RaiseException(context, WG_EXC_INDEXERROR);
				return nullptr;
			}

			buf[index] = argv[2];
			return Wg_None(context);
		}
		
		static Wg_Obj* list_append(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_LIST(0);

			argv[0]->Get<std::vector<Wg_Obj*>>().push_back(argv[1]);
			return Wg_None(context);
		}

		static Wg_Obj* list_insert(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(3);
			WG_EXPECT_ARG_TYPE_LIST(0);
			WG_EXPECT_ARG_TYPE_INT(1);

			Wg_int index;
			if (!AbsIndex(argv[0], argv[1], index))
				return nullptr;

			auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();			
			index = std::clamp(index, (Wg_int)0, (Wg_int)buf.size() + 1);
			buf.insert(buf.begin() + index, argv[2]);
			return Wg_None(context);
		}

		static Wg_Obj* list_pop(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 2);
			WG_EXPECT_ARG_TYPE_LIST(0);

			auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();
			Wg_int index = (Wg_int)buf.size() - 1;
			if (argc == 2) {
				WG_EXPECT_ARG_TYPE_INT(1);
				if (!AbsIndex(argv[0], argv[1], index))
					return nullptr;
			}

			if (index < 0 || index >= (Wg_int)buf.size()) {
				Wg_RaiseException(context, WG_EXC_INDEXERROR);
				return nullptr;
			}
			
			Wg_Obj* popped = buf[index];
			buf.erase(buf.begin() + index);
			return popped;
		}

		static Wg_Obj* list_remove(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_LIST(0);

			auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();
			for (size_t i = 0; i < buf.size(); i++) {
				Wg_Obj* eq = Wg_BinaryOp(WG_BOP_EQ, argv[1], buf[i]);
				if (eq == nullptr)
					return nullptr;
				
				if (Wg_GetBool(eq)) {
					if (i < buf.size())
						buf.erase(buf.begin() + i);
					return Wg_None(context);
				}
			}

			Wg_RaiseException(context, WG_EXC_VALUEERROR, "Value was not found");
			return nullptr;
		}

		static Wg_Obj* list_clear(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_LIST(0);

			argv[0]->Get<std::vector<Wg_Obj*>>().clear();
			return Wg_None(context);
		}

		static Wg_Obj* list_copy(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_LIST(0);

			auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();
			return Wg_NewList(context, buf.data(), !buf.size());
		}

		static Wg_Obj* list_extend(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_LIST(0);

			auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();

			if (argv[0] == argv[1]) {
				// Double the list instead of going into an infinite loop
				buf.insert(buf.end(), buf.begin(), buf.end());
			} else {
				bool success = Wg_Iterate(argv[1], &buf, [](Wg_Obj* value, void* ud) {
					std::vector<Wg_Obj*>& buf = *(std::vector<Wg_Obj*>*)ud;
					buf.push_back(value);
					return true;
					});
				if (!success)
					return nullptr;
			}
			
			return Wg_None(context);
		}

		static Wg_Obj* list_sort(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_LIST(0);

			Wg_Obj* kwargs = Wg_GetKwargs(context);
			if (kwargs == nullptr)
				return nullptr;

			Wg_Obj* kw[2]{};
			const char* keys[2] = { "reverse", "key" };
			if (!Wg_ParseKwargs(kwargs, keys, 2, kw))
				return nullptr;

			bool reverse = false;
			if (kw[0] != nullptr) {
				Wg_Obj* reverseValue = Wg_UnaryOp(WG_UOP_BOOL, kw[0]);
				if (reverseValue == nullptr)
					return nullptr;
				reverse = Wg_GetBool(reverseValue);
			}

			std::vector<Wg_Obj*> buf = argv[0]->Get<std::vector<Wg_Obj*>>();
			std::vector<Wg_ObjRef> refs;
			for (Wg_Obj* v : buf)
				refs.emplace_back(v);

			if (!MergeSort(buf.data(), buf.size(), kw[1]))
				return nullptr;

			if (reverse)
				std::reverse(buf.begin(), buf.end());
			
			argv[0]->Get<std::vector<Wg_Obj*>>() = std::move(buf);

			return Wg_None(context);
		}

		static Wg_Obj* list_reverse(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_LIST(0);

			auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();
			std::reverse(buf.begin(), buf.end());
			return Wg_None(context);
		}

		static Wg_Obj* map_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_MAP(0);

			auto it = std::find(context->reprStack.rbegin(), context->reprStack.rend(), argv[0]);
			if (it != context->reprStack.rend()) {
				return Wg_NewString(context, "{...}");
			} else {
				context->reprStack.push_back(argv[0]);
				const auto& buf = argv[0]->Get<WDict>();
				std::string s = "{";
				for (const auto& [key, val] : buf) {
					Wg_Obj* k = Wg_UnaryOp(WG_UOP_REPR, key);
					if (k == nullptr) {
						context->reprStack.pop_back();
						return nullptr;
					}
					s += k->Get<std::string>() + ": ";
					
					Wg_Obj* v = Wg_UnaryOp(WG_UOP_REPR, val);
					if (v == nullptr) {
						context->reprStack.pop_back();
						return nullptr;
					}
					s += v->Get<std::string>() + ", ";
				}
				context->reprStack.pop_back();
				if (!buf.empty()) {
					s.pop_back();
					s.pop_back();
				}
				return Wg_NewString(context, (s + "}").c_str());
			}
		}

		static Wg_Obj* map_nonzero(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_MAP(0);
			return Wg_NewBool(context, !argv[0]->Get<WDict>().empty());
		}

		static Wg_Obj* map_len(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_MAP(0);
			return Wg_NewInt(context, (Wg_int)argv[0]->Get<WDict>().size());
		}

		static Wg_Obj* map_contains(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_MAP(0);
			try {
				return Wg_NewBool(context, argv[0]->Get<WDict>().contains(argv[1]));
			} catch (HashException&) {
				return nullptr;
			}
		}

		static Wg_Obj* map_iter(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_MAP(0);
			return Wg_Call(context->builtins.dictKeysIter, argv, 1, nullptr);
		}

		static Wg_Obj* map_values(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_MAP(0);
			return Wg_Call(context->builtins.dictValuesIter, argv, 1, nullptr);
		}

		static Wg_Obj* map_items(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_MAP(0);
			return Wg_Call(context->builtins.dictItemsIter, argv, 1, nullptr);
		}

		static Wg_Obj* map_get(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(2, 3);
			WG_EXPECT_ARG_TYPE_MAP(0);

			auto& buf = argv[0]->Get<WDict>();
			WDict::iterator it;
			try {
				it = buf.find(argv[1]);
			} catch (HashException&) {
				return nullptr;
			}

			if (it == buf.end()) {
				return argc == 3 ? argv[2] : Wg_None(context);
			}

			return it->second;
		}

		static Wg_Obj* map_getitem(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_MAP(0);

			auto& buf = argv[0]->Get<WDict>();
			WDict::iterator it;
			try {
				it = buf.find(argv[1]);
			} catch (HashException&) {
				return nullptr;
			}

			if (it == buf.end()) {
				Wg_RaiseKeyError(context, argv[1]);
				return nullptr;
			}

			return it->second;
		}

		static Wg_Obj* map_setitem(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(3);
			WG_EXPECT_ARG_TYPE_MAP(0);

			try {
				argv[0]->Get<WDict>()[argv[1]] = argv[2];
			} catch (HashException&) {
				return nullptr;
			}
			return Wg_None(context);
		}

		static Wg_Obj* map_clear(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_MAP(0);
			argv[0]->Get<WDict>().clear();
			return Wg_None(context);
		}

		static Wg_Obj* map_copy(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_MAP(0);

			std::vector<Wg_Obj*> keys;
			std::vector<Wg_Obj*> values;
			for (const auto& [k, v] : argv[0]->Get<WDict>()) {
				keys.push_back(k);
				values.push_back(v);
			}
			return Wg_NewDictionary(context, keys.data(), values.data(), (int)keys.size());
		}

		static Wg_Obj* map_pop(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(2, 3);
			WG_EXPECT_ARG_TYPE_MAP(0);

			if (auto popped = argv[0]->Get<WDict>().erase(argv[1]))
				return popped.value();

			if (argc == 3)
				return argv[2];

			Wg_RaiseKeyError(context, argv[1]);
			return nullptr;
		}

		static Wg_Obj* map_popitem(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_MAP(0);

			auto& buf = argv[0]->Get<WDict>();
			if (buf.empty()) {
				Wg_RaiseKeyError(context);
				return nullptr;
			}

			auto popped = buf.pop();
			Wg_Obj* tupElems[2] = { popped.first, popped.second };
			return Wg_NewTuple(context, tupElems, 2);
		}

		static Wg_Obj* map_setdefault(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(2, 3);
			WG_EXPECT_ARG_TYPE_MAP(0);

			try {
				auto& entry = argv[0]->Get<WDict>()[argv[1]];
				if (entry == nullptr)
					entry = argc == 3 ? argv[2] : Wg_None(context);
				return entry;
			} catch (HashException&) {
				return nullptr;
			}
		}

		static Wg_Obj* map_update(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_MAP(0);

			Wg_Obj* iterable = argv[1];
			if (Wg_IsDictionary(argv[1])) {
				iterable = Wg_CallMethod(argv[1], "items", nullptr, 0);
			}

			auto f = [](Wg_Obj* obj, void* ud) {
				Wg_Obj* kv[2]{};
				if (!Wg_Unpack(obj, 2, kv))
					return false;
				
				Wg_ObjRef ref(kv[1]);
				try {
					((Wg_Obj*)ud)->Get<WDict>()[kv[0]] = kv[1];
				} catch (HashException&) {}
				return true;
			};
			
			if (Wg_Iterate(iterable, argv[0], f)) {
				return Wg_None(context);
			} else {
				return nullptr;
			}
		}

		static Wg_Obj* set_nonzero(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_SET(0);
			return Wg_NewBool(context, !argv[0]->Get<WSet>().empty());
		}

		static Wg_Obj* set_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_SET(0);

			auto it = std::find(context->reprStack.rbegin(), context->reprStack.rend(), argv[0]);
			if (it != context->reprStack.rend()) {
				return Wg_NewString(context, "{...}");
			} else {
				context->reprStack.push_back(argv[0]);
				const auto& buf = argv[0]->Get<WSet>();

				if (buf.empty()) {
					context->reprStack.pop_back();
					return Wg_NewString(context, "set()");
				}

				std::string s = "{";
				for (Wg_Obj* val : buf) {
					Wg_Obj* v = Wg_UnaryOp(WG_UOP_REPR, val);
					if (v == nullptr) {
						context->reprStack.pop_back();
						return nullptr;
					}
					s += v->Get<std::string>() + ", ";
				}
				context->reprStack.pop_back();
				if (!buf.empty()) {
					s.pop_back();
					s.pop_back();
				}
				return Wg_NewString(context, (s + "}").c_str());
			}
		}

		static Wg_Obj* set_iter(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_SET(0);
			return Wg_Call(context->builtins.setIter, argv, 1, nullptr);
		}

		static Wg_Obj* set_contains(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_SET(0);
			try {
				return Wg_NewBool(context, argv[0]->Get<WSet>().contains(argv[1]));
			} catch (HashException&) {
				Wg_ClearException(context);
				return Wg_NewBool(context, false);
			}
		}

		static Wg_Obj* set_len(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_SET(0);
			return Wg_NewInt(context, (int)argv[0]->Get<WSet>().size());
		}

		static Wg_Obj* set_clear(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_SET(0);
			argv[0]->Get<WSet>().clear();
			return Wg_None(context);
		}

		static Wg_Obj* set_copy(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_SET(0);
			return Wg_Call(context->builtins.set, argv, 1);
		}
		
		static Wg_Obj* set_add(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_SET(0);
			argv[0]->Get<WSet>().insert(argv[1]);
			return Wg_None(context);
		}

		static Wg_Obj* set_remove(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_SET(0);
			
			WSet::const_iterator it{};
			auto& set = argv[0]->Get<WSet>();
			try {
				it = set.find(argv[1]);
			} catch (HashException&) {
				return nullptr;
			}

			if (it == WSet::const_iterator{}) {
				Wg_RaiseKeyError(context, argv[1]);
				return nullptr;
			} else {
				set.erase(it);
				return Wg_None(context);
			}
		}

		static Wg_Obj* set_discard(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_SET(0);

			WSet::const_iterator it{};
			auto& set = argv[0]->Get<WSet>();
			try {
				it = set.find(argv[1]);
			} catch (HashException&) {
				return nullptr;
			}

			if (it != WSet::const_iterator{})
				set.erase(it);
			return Wg_None(context);
		}

		static Wg_Obj* set_pop(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_SET(0);
			auto& set = argv[0]->Get<WSet>();
			auto it = set.begin();
			if (it == set.end()) {
				Wg_RaiseKeyError(context);
				return nullptr;
			}
			Wg_Obj* obj = *it;
			set.erase(set.begin());
			return obj;
		}

		static Wg_Obj* set_update(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_SET(0);

			auto f = [](Wg_Obj* obj, void* ud) {
				auto set = (WSet*)ud;
				try {
					set->insert(obj);
				} catch (HashException&) {}
				return true;
			};

			if (!Wg_Iterate(argv[1], &argv[0]->Get<WSet>(), f))
				return nullptr;
			
			return Wg_None(context);
		}

		static Wg_Obj* set_union(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_AT_LEAST(1);
			WG_EXPECT_ARG_TYPE_SET(0);

			Wg_Obj* res = Wg_NewSet(context);
			Wg_ObjRef ref(res);
			
			auto f = [](Wg_Obj* obj, void* ud) {
				try {
					((WSet*)ud)->insert(obj);
				} catch (HashException&) {}
				return true;
			};

			for (int i = 0; i < argc; i++)
				if (!Wg_Iterate(argv[i], &res->Get<WSet>(), f))
					return nullptr;

			return res;
		}

		static Wg_Obj* set_difference(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_AT_LEAST(1);
			WG_EXPECT_ARG_TYPE_SET(0);

			Wg_Obj* res = Wg_NewSet(context);
			Wg_ObjRef ref(res);

			struct State {
				Wg_Obj** other;
				int otherCount;
				WSet* res;
			} s{ argv + 1, argc - 1, &res->Get<WSet>() };

			auto f = [](Wg_Obj* obj, void* ud) {
				auto s = (State*)ud;

				for (int i = 0; i < s->otherCount; i++) {
					Wg_Obj* contains = Wg_BinaryOp(WG_BOP_IN, obj, s->other[i]);
					if (contains == nullptr)
						return false;
					else if (Wg_GetBool(contains))
						return true;
				}

				try {
					s->res->insert(obj);
				} catch (HashException&) {}
				return true;
			};

			if (!Wg_Iterate(argv[0], &s, f))
				return nullptr;

			return res;
		}

		static Wg_Obj* set_intersection(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_AT_LEAST(1);
			WG_EXPECT_ARG_TYPE_SET(0);

			Wg_Obj* res = Wg_NewSet(context);
			Wg_ObjRef ref(res);

			struct State {
				Wg_Obj** other;
				int otherCount;
				WSet* res;
			} s{ argv + 1, argc - 1, &res->Get<WSet>() };

			auto f = [](Wg_Obj* obj, void* ud) {
				auto s = (State*)ud;

				for (int i = 0; i < s->otherCount; i++) {
					Wg_Obj* contains = Wg_BinaryOp(WG_BOP_IN, obj, s->other[i]);
					if (contains == nullptr)
						return false;
					else if (!Wg_GetBool(contains))
						return true;
				}

				try {
					s->res->insert(obj);
				} catch (HashException&) {}
				return true;
			};

			if (!Wg_Iterate(argv[0], &s, f))
				return nullptr;

			return res;
		}

		static Wg_Obj* set_symmetric_difference(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_SET(0);

			Wg_Obj* res = Wg_NewSet(context);
			Wg_ObjRef ref(res);

			struct State {
				Wg_Obj* other;
				WSet* res;
			} s = { nullptr, &res->Get<WSet>() };

			auto f = [](Wg_Obj* obj, void* ud) {
				auto s = (State*)ud;

				Wg_Obj* contains = Wg_BinaryOp(WG_BOP_IN, obj, s->other);
				if (contains == nullptr)
					return false;
				else if (Wg_GetBool(contains))
					return true;
					
				try {
					s->res->insert(obj);
				} catch (HashException&) {}
				return true;
			};

			s.other = argv[1];
			if (!Wg_Iterate(argv[0], &s, f))
				return nullptr;
			s.other = argv[0];
			if (!Wg_Iterate(argv[1], &s, f))
				return nullptr;

			return res;
		}

		static Wg_Obj* set_isdisjoint(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_SET(0);

			Wg_Obj* inters = Wg_CallMethod(argv[0], "intersection", argv + 1, 1);
			if (inters == nullptr)
				return nullptr;

			return Wg_UnaryOp(WG_UOP_NOT, inters);
		}

		static Wg_Obj* set_issubset(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_SET(0);

			size_t size = argv[0]->Get<WSet>().size();

			Wg_Obj* inters = Wg_CallMethod(argv[0], "intersection", argv + 1, 1);
			if (inters == nullptr)
				return nullptr;
			
			if (!Wg_IsSet(inters)) {
				return Wg_NewBool(context, false);
			}

			return Wg_NewBool(context, inters->Get<WSet>().size() == size);
		}

		static Wg_Obj* set_issuperset(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_SET(0);

			struct State {
				Wg_Obj* self;
				bool result;
			} s = { argv[0], true };

			auto f = [](Wg_Obj* obj, void* ud) {
				auto s = (State*)ud;
				Wg_Obj* contains = Wg_BinaryOp(WG_BOP_IN, obj, s->self);
				if (contains && !Wg_GetBool(contains)) {
					s->result = false;
					return false;
				}
				return true;
			};

			if (!Wg_Iterate(argv[1], &s, f) && s.result)
				return nullptr;
			
			return Wg_NewBool(context, s.result);
		}

		static Wg_Obj* BaseException_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			return Wg_GetAttribute(argv[0], "_message");
		}

		static Wg_Obj* DictKeysIter_next(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WDict::iterator* it{};
			if (!TryGetUserdata(argv[0], "__DictKeysIter", &it)) {
				Wg_RaiseArgumentTypeError(context, 0, "__DictKeysIter");
				return nullptr;
			}
			
			it->Revalidate();
			if (*it == WDict::iterator{}) {
				Wg_RaiseException(context, WG_EXC_STOPITERATION);
				return nullptr;
			}
			
			Wg_Obj* key = (*it)->first;
			++(*it);
			return key;
		}

		static Wg_Obj* DictValuesIter_next(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WDict::iterator* it{};
			if (!TryGetUserdata(argv[0], "__DictValuesIter", &it)) {
				Wg_RaiseArgumentTypeError(context, 0, "__DictValuesIter");
				return nullptr;
			}

			it->Revalidate();
			if (*it == WDict::iterator{}) {
				Wg_RaiseException(context, WG_EXC_STOPITERATION);
				return nullptr;
			}

			Wg_Obj* value = (*it)->second;
			++(*it);
			return value;
		}

		static Wg_Obj* DictItemsIter_next(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WDict::iterator* it{};
			if (!TryGetUserdata(argv[0], "__DictItemsIter", &it)) {
				Wg_RaiseArgumentTypeError(context, 0, "__DictItemsIter");
				return nullptr;
			}
			
			it->Revalidate();
			if (*it == WDict::iterator{}) {
				Wg_RaiseException(context, WG_EXC_STOPITERATION);
				return nullptr;
			}

			Wg_Obj* tup[2] = { (*it)->first, (*it)->second };
			++(*it);
			return Wg_NewTuple(context, tup, 2);
		}

		static Wg_Obj* SetIter_next(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WSet::iterator* it{};
			if (!TryGetUserdata(argv[0], "__SetIter", &it)) {
				Wg_RaiseArgumentTypeError(context, 0, "__SetIter");
				return nullptr;
			}
			
			it->Revalidate();
			if (*it == WSet::iterator{}) {
				Wg_RaiseException(context, WG_EXC_STOPITERATION);
				return nullptr;
			}
			
			Wg_Obj* obj = **it;
			++(*it);
			return obj;
		}

		static Wg_Obj* File_iter(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			if (!Wg_TryGetUserdata(argv[0], "__File", nullptr)) {
				Wg_RaiseArgumentTypeError(context, 0, "__File");
				return nullptr;
			}

			return Wg_Call(context->builtins.readlineIter, argv, 1);
		}

		static Wg_Obj* File_read(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 2);
			std::fstream* f{};
			if (!TryGetUserdata(argv[0], "__File", &f)) {
				Wg_RaiseArgumentTypeError(context, 0, "__File");
				return nullptr;
			}

			Wg_int size = -1;
			if (argc == 2) {
				WG_EXPECT_ARG_TYPE_INT(1);
				size = Wg_GetInt(argv[1]);
			}

			if (size < 0) {
				auto cur = f->tellg();
				f->seekg(0, std::ios::end);
				size = (Wg_int)(f->tellg() - cur);
				f->seekg(cur);
			}
			
			std::vector<char> buf((size_t)size);
			f->read(buf.data(), size);
			return Wg_NewStringBuffer(context, buf.data(), (int)buf.size());
		}

		static Wg_Obj* File_readline(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			std::fstream* f{};
			if (!TryGetUserdata(argv[0], "__File", &f)) {
				Wg_RaiseArgumentTypeError(context, 0, "__File");
				return nullptr;
			}
			
			if (f->eof()) {
				return Wg_NewString(context);
			}

			std::string s;
			std::getline(*f, s);
			if (f->good())
				s += '\n';
			return Wg_NewString(context, s.c_str());
		}

		static Wg_Obj* File_readlines(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			if (!Wg_TryGetUserdata(argv[0], "__File", nullptr)) {
				Wg_RaiseArgumentTypeError(context, 0, "__File");
				return nullptr;
			}

			return Wg_Call(context->builtins.list, argv, 1);
		}

		static Wg_Obj* File_closex(Wg_Context* context, Wg_Obj** argv, int) {
			std::fstream* f{};
			if (!TryGetUserdata(argv[0], "__File", &f)) {
				Wg_RaiseArgumentTypeError(context, 0, "__File");
				return nullptr;
			}

			if (f->is_open())
				f->close();

			return Wg_None(context);
		}

		static Wg_Obj* File_close(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			return File_closex(context, argv, argc);
		}

		static Wg_Obj* File_exit(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(4);
			return File_closex(context, argv, argc);
		}

		static Wg_Obj* File_seekable(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			if (!Wg_TryGetUserdata(argv[0], "__File", nullptr)) {
				Wg_RaiseArgumentTypeError(context, 0, "__File");
				return nullptr;
			}
			
			return Wg_NewBool(context, true);
		}

		static Wg_Obj* File_readable(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			if (!Wg_TryGetUserdata(argv[0], "__File", nullptr)) {
				Wg_RaiseArgumentTypeError(context, 0, "__File");
				return nullptr;
			}

			return Wg_GetAttribute(argv[0], "_readable");
		}

		static Wg_Obj* File_writable(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			if (!Wg_TryGetUserdata(argv[0], "__File", nullptr)) {
				Wg_RaiseArgumentTypeError(context, 0, "__File");
				return nullptr;
			}

			return Wg_GetAttribute(argv[0], "_writable");
		}

		static Wg_Obj* File_seek(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(1);
			std::fstream* f{};
			if (!TryGetUserdata(argv[0], "__File", &f)) {
				Wg_RaiseArgumentTypeError(context, 0, "__File");
				return nullptr;
			}
			
			f->seekg((std::streampos)Wg_GetInt(argv[1]));

			return Wg_None(context);
		}

		static Wg_Obj* File_tell(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			std::fstream* f{};
			if (!TryGetUserdata(argv[0], "__File", &f)) {
				Wg_RaiseArgumentTypeError(context, 0, "__File");
				return nullptr;
			}

			return Wg_NewInt(context, (Wg_int)f->tellg());
		}

		static Wg_Obj* File_flush(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			std::fstream* f{};
			if (!TryGetUserdata(argv[0], "__File", &f)) {
				Wg_RaiseArgumentTypeError(context, 0, "__File");
				return nullptr;
			}

			f->flush();

			return Wg_None(context);
		}

		static Wg_Obj* File_write(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_STRING(1);
			std::fstream* f{};
			if (!TryGetUserdata(argv[0], "__File", &f)) {
				Wg_RaiseArgumentTypeError(context, 0, "__File");
				return nullptr;
			}

			int len{};
			const char* s = Wg_GetString(argv[1], &len);
			f->write(s, (std::streamsize)len);

			return Wg_None(context);
		}

		static Wg_Obj* File_writelines(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			if (!Wg_TryGetUserdata(argv[0], "__File", nullptr)) {
				Wg_RaiseArgumentTypeError(context, 0, "__File");
				return nullptr;
			}

			auto fn = [](Wg_Obj* obj, void* ud) {
				auto* file = (Wg_Obj*)ud;
				return Wg_CallMethod(file, "write", &obj, 1) != nullptr;
			};

			if (!Wg_Iterate(argv[1], argv[0], fn))
				return nullptr;

			return Wg_None(context);
		}
		
		static Wg_Obj* self(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			return argv[0];
		}

	} // namespace methods

	namespace lib {

		template <size_t base>
		static Wg_Obj* base_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);

			Wg_Obj* val = Wg_UnaryOp(WG_UOP_INDEX, argv[0]);
			if (val == nullptr)
				return nullptr;
			
			Wg_int i = Wg_GetInt(val);			
			std::string s;

			if constexpr (base == 2) {
				s = "0b";
			} else if constexpr (base == 8) {
				s = "0o";
			} else if constexpr (base == 16) {
				s = "0x";
			}
			
			do {
				s += "0123456789abcdef"[i % base];
				i /= base;
			}
			while (i > 0);

			return Wg_NewString(context, s.c_str());
		}

		static Wg_Obj* callable(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			
			if (Wg_IsFunction(argv[0])) {
				return Wg_NewBool(context, true);
			} else {
				return Wg_NewBool(context, Wg_HasAttribute(argv[0], "__call__"));
			}
		}

		static Wg_Obj* chr(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_INT(0);
			
			Wg_int i = Wg_GetInt(argv[0]);
			char s[2] = { (char)i, 0 };
			return Wg_NewString(context, s);
		}

		static Wg_Obj* compile(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(3);
			WG_EXPECT_ARG_TYPE_STRING(0);
			WG_EXPECT_ARG_TYPE_STRING(1);
			WG_EXPECT_ARG_TYPE_STRING(2);
		
			const char* source = Wg_GetString(argv[0]);
			const char* filename = Wg_GetString(argv[1]);
			const char* mode = Wg_GetString(argv[2]);
			
			Wg_Obj* fn = nullptr;
			if (std::strcmp(mode, "exec")) {
				fn = Wg_Compile(context, source, filename);
			} else if (std::strcmp(mode, "eval")) {
				fn = Wg_CompileExpression(context, source, filename);
			} else {
				Wg_RaiseException(context, WG_EXC_VALUEERROR, "compile() mode must be 'exec' or 'eval'");
			}
			
			if (fn == nullptr)
				return nullptr;

			return Wg_Call(context->builtins.codeObject, &fn, 1);
		}

		static Wg_Obj* eval(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			
			if (Wg_IsInstance(argv[0], &context->builtins.codeObject, 1)) {
				return Wg_CallMethod(argv[0], "f", nullptr, 0);
			} else {
				WG_EXPECT_ARG_TYPE_STRING(0);
				const char* source = Wg_GetString(argv[0]);
				return Wg_ExecuteExpression(context, source, "<string>");
			}
		}

		static Wg_Obj* exec(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);

			if (Wg_IsInstance(argv[0], &context->builtins.codeObject, 1)) {
				if (Wg_CallMethod(argv[0], "f", nullptr, 0) == nullptr)
					return nullptr;
			} else {
				WG_EXPECT_ARG_TYPE_STRING(0);
				const char* source = Wg_GetString(argv[0]);
				if (Wg_Execute(context, source, "<string>")) {
					return Wg_None(context);
				} else {
					return nullptr;
				}
			}
			return Wg_None(context);
		}

		static Wg_Obj* exit(Wg_Context* context, Wg_Obj**, int) {
			Wg_RaiseException(context, WG_EXC_SYSTEMEXIT);
			return nullptr;
		}

		static Wg_Obj* getattr(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_STRING(1);
			
			const char* name = Wg_GetString(argv[1]);
			return Wg_GetAttribute(argv[0], name);
		}
		
		static Wg_Obj* id(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			return Wg_NewInt(context, (Wg_int)argv[0]);
		}

		static Wg_Obj* input(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(0, 1);
			
			if (argc == 1) {
				Wg_Obj* prompt = Wg_UnaryOp(WG_UOP_STR, argv[0]);
				if (prompt == nullptr)
					return nullptr;
				
				Wg_PrintString(context, Wg_GetString(prompt));
			}
			
			std::string s;
			std::getline(std::cin, s);
			
			return Wg_NewString(context, s.c_str());
		}

		static Wg_Obj* isinstance(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			bool ret{};
			if (Wg_IsTuple(argv[1])) {
				const auto& buf = argv[1]->Get<std::vector<Wg_Obj*>>();
				ret = Wg_IsInstance(argv[0], buf.data(), (int)buf.size()) != nullptr;
			} else {
				ret = Wg_IsInstance(argv[0], argv + 1, 1) != nullptr;
			}
			return Wg_NewBool(context, ret);
		}

		static Wg_Obj* ord(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_STRING(0);

			const char* s = Wg_GetString(argv[0]);
			if (s[0] == 0) {
				Wg_RaiseException(context, WG_EXC_VALUEERROR, "ord() arg is an empty string");
				return nullptr;
			} else if (s[1] == 0) {
				return Wg_NewInt(context, (int)s[0]);
			} else {
				Wg_RaiseException(context, WG_EXC_VALUEERROR, "ord() arg is not a single character");
				return nullptr;
			}
		}

		static Wg_Obj* pow(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			return Wg_BinaryOp(WG_BOP_POW, argv[0], argv[1]);
		}

		static Wg_Obj* print(Wg_Context* context, Wg_Obj** argv, int argc) {
			Wg_Obj* kwargs = Wg_GetKwargs(context);
			if (kwargs == nullptr)
				return nullptr;
			
			Wg_Obj* kw[3]{};
			const char* keys[3] = { "sep", "end", "flush" };
			if (!Wg_ParseKwargs(kwargs, keys, 3, kw))
				return nullptr;

			std::string sep = " ";
			std::string end = "\n";
			if (kw[0] && !Wg_IsNone(kw[0]))
				sep = Wg_GetString(kw[0]);
			if (kw[1] && !Wg_IsNone(kw[1]))
				end = Wg_GetString(kw[1]);

			std::string text;
			for (int i = 0; i < argc; i++) {
				if (Wg_Obj* s = Wg_UnaryOp(WG_UOP_STR, argv[i])) {
					text += Wg_GetString(s);
				} else {
					return nullptr;
				}

				if (i < argc - 1) {
					text += sep;
				}
			}
			text += end;
			Wg_Print(context, text.c_str(), (int)text.size());
			return Wg_None(context);
		}

		static Wg_Obj* round(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 2);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);

			Wg_float f = Wg_GetFloat(argv[0]);
			
			Wg_float m = 1;
			bool dpSpecified = false;
			if (argc == 2 && !Wg_IsNone(argv[1])) {
				WG_EXPECT_ARG_TYPE_INT(1);
				m = std::pow(10, Wg_GetInt(argv[1]));
				dpSpecified = true;
			}
			
			auto r = std::lrint(f * m) / m;
			if (!dpSpecified || Wg_IsInt(argv[0])) {
				return Wg_NewInt(context, (Wg_int)r);
			} else {
				return Wg_NewFloat(context, (Wg_float)r);
			}
		}

		static Wg_Obj* setattr(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(3);
			WG_EXPECT_ARG_TYPE_STRING(1);

			const char* name = Wg_GetString(argv[1]);
			Wg_SetAttribute(argv[0], name, argv[2]);
			return Wg_None(context);
		}

	} // namespace lib

	bool ImportBuiltins(Wg_Context* context) {
		try {
			auto getGlobal = [&](const char* name) {
				if (Wg_Obj* v = Wg_GetGlobal(context, name))
					return v;
				throw LibraryInitException();
			};

			auto createClass = [&](const char* name, Wg_Obj* base = nullptr, bool assign = true) {
				if (Wg_Obj* v = Wg_NewClass(context, name, &base, base ? 1 : 0)) {
					if (assign)
						Wg_SetGlobal(context, name, v);
					return v;
				}
				throw LibraryInitException();
			};

			auto& b = context->builtins;
			Wg_Obj::Class* klass = nullptr;

			// Create object class
			b.object = Alloc(context);
			if (b.object == nullptr)
				throw LibraryInitException();
			b.object->type = "__class";
			klass = new Wg_Obj::Class{ std::string("object") };
			Wg_SetUserdata(b.object, klass);
			Wg_RegisterFinalizer(b.object, [](void* ud) { delete (Wg_Obj::Class*)ud; }, klass);
			b.object->Get<Wg_Obj::Class>().instanceAttributes.Set("__class__", b.object);
			b.object->attributes.AddParent(b.object->Get<Wg_Obj::Class>().instanceAttributes);
			b.object->Get<Wg_Obj::Class>().userdata = context;
			b.object->Get<Wg_Obj::Class>().ctor = ctors::object;
			Wg_SetGlobal(context, "object", b.object);

			// Create function class
			b.func = Alloc(context);
			if (b.func == nullptr)
				throw LibraryInitException();
			b.func->type = "__class";
			klass = new Wg_Obj::Class{ std::string("function") };
			Wg_SetUserdata(b.func, klass);
			Wg_RegisterFinalizer(b.func, [](void* ud) { delete (Wg_Obj::Class*)ud; }, klass);
			b.func->Get<Wg_Obj::Class>().instanceAttributes.Set("__class__", b.func);
			b.func->Get<Wg_Obj::Class>().instanceAttributes.AddParent(b.object->Get<Wg_Obj::Class>().instanceAttributes);
			b.func->attributes.AddParent(b.object->Get<Wg_Obj::Class>().instanceAttributes);
			b.func->Get<Wg_Obj::Class>().userdata = context;
			b.func->Get<Wg_Obj::Class>().ctor = [](Wg_Context* context, Wg_Obj**, int) -> Wg_Obj* {
				Wg_RaiseException(context, WG_EXC_TYPEERROR, "A function cannot be created directly");
				return nullptr;
			};

			// Create tuple class
			b.tuple = Alloc(context);
			b.tuple->type = "__class";
			klass = new Wg_Obj::Class{ std::string("tuple") };
			Wg_SetUserdata(b.tuple, klass);
			Wg_RegisterFinalizer(b.tuple, [](void* ud) { delete (Wg_Obj::Class*)ud; }, klass);
			b.tuple->Get<Wg_Obj::Class>().instanceAttributes.Set("__class__", b.tuple);
			b.tuple->Get<Wg_Obj::Class>().instanceAttributes.AddParent(b.object->Get<Wg_Obj::Class>().instanceAttributes);
			b.tuple->attributes.AddParent(b.object->Get<Wg_Obj::Class>().instanceAttributes);
			b.tuple->Get<Wg_Obj::Class>().userdata = context;
			b.tuple->Get<Wg_Obj::Class>().ctor = ctors::tuple;
			Wg_SetGlobal(context, "tuple", b.tuple);
			RegisterMethod(b.tuple, "__mul__", methods::collection_mul<Collection::Tuple>);
			RegisterMethod(b.tuple, "__iter__", methods::object_iter);
			RegisterMethod(b.tuple, "__str__", methods::collection_str<Collection::Tuple>);
			RegisterMethod(b.tuple, "__getitem__", methods::collection_getitem<Collection::Tuple>);
			RegisterMethod(b.tuple, "__len__", methods::collection_len<Collection::Tuple>);
			RegisterMethod(b.tuple, "__contains__", methods::collection_contains<Collection::Tuple>);
			RegisterMethod(b.tuple, "__eq__", methods::collection_eq<Collection::Tuple>);
			RegisterMethod(b.tuple, "__lt__", methods::collection_lt<Collection::Tuple>);
			RegisterMethod(b.tuple, "__nonzero__", methods::collection_nonzero<Collection::Tuple>);
			RegisterMethod(b.tuple, "count", methods::collection_count<Collection::Tuple>);
			RegisterMethod(b.tuple, "index", methods::collection_index<Collection::Tuple>);

			// Create NoneType class
			b.noneType = Alloc(context);
			b.noneType->type = "__class";
			klass = new Wg_Obj::Class{ std::string("NoneType") };
			Wg_SetUserdata(b.noneType, klass);
			Wg_RegisterFinalizer(b.noneType, [](void* ud) { delete (Wg_Obj::Class*)ud; }, klass);
			b.noneType->attributes.AddParent(b.object->Get<Wg_Obj::Class>().instanceAttributes);
			b.noneType->Get<Wg_Obj::Class>().userdata = context;
			b.noneType->Get<Wg_Obj::Class>().ctor = ctors::none;

			// Create None singleton
			b.none = Alloc(context);
			b.none->type = "__null";
			Wg_SetAttribute(b.none, "__class__", b.none);
			b.none->attributes.AddParent(b.object->Get<Wg_Obj::Class>().instanceAttributes);
			RegisterMethod(b.none, "__nonzero__", methods::null_nonzero);
			RegisterMethod(b.none, "__str__", methods::null_str);

			// Add __bases__ tuple to the classes created before
			Wg_Obj* emptyTuple = Wg_NewTuple(context, nullptr, 0);
			if (emptyTuple == nullptr)
				throw LibraryInitException();
			Wg_Obj* objectTuple = Wg_NewTuple(context, &b.object, 1);
			if (objectTuple == nullptr)
				throw LibraryInitException();
			Wg_SetAttribute(b.object, "__bases__", emptyTuple);
			Wg_SetAttribute(b.none, "__bases__", objectTuple);
			Wg_SetAttribute(b.func, "__bases__", objectTuple);
			Wg_SetAttribute(b.tuple, "__bases__", objectTuple);

			// Add methods
			RegisterMethod(b.object, "__pos__", methods::self);
			RegisterMethod(b.object, "__str__", methods::object_str);
			RegisterMethod(b.object, "__nonzero__", methods::object_nonzero);
			RegisterMethod(b.object, "__repr__", methods::object_repr);
			RegisterMethod(b.object, "__eq__", methods::object_eq);
			RegisterMethod(b.object, "__ne__", methods::object_ne);
			RegisterMethod(b.object, "__le__", methods::object_le);
			RegisterMethod(b.object, "__gt__", methods::object_gt);
			RegisterMethod(b.object, "__ge__", methods::object_ge);
			RegisterMethod(b.object, "__iadd__", methods::object_iadd);
			RegisterMethod(b.object, "__isub__", methods::object_isub);
			RegisterMethod(b.object, "__imul__", methods::object_imul);
			RegisterMethod(b.object, "__itruediv__", methods::object_itruediv);
			RegisterMethod(b.object, "__ifloordiv__", methods::object_ifloordiv);
			RegisterMethod(b.object, "__imod__", methods::object_imod);
			RegisterMethod(b.object, "__ipow__", methods::object_ipow);
			RegisterMethod(b.object, "__iand__", methods::object_iand);
			RegisterMethod(b.object, "__ior__", methods::object_ior);
			RegisterMethod(b.object, "__ixor__", methods::object_ixor);
			RegisterMethod(b.object, "__ilshift__", methods::object_ilshift);
			RegisterMethod(b.object, "__irshift__", methods::object_irshift);
			RegisterMethod(b.object, "__hash__", methods::object_hash);
			RegisterMethod(b.object, "__iter__", methods::object_iter);
			RegisterMethod(b.object, "__reversed__", methods::object_reversed);

			b._bool = createClass("bool");
			b._bool->Get<Wg_Obj::Class>().ctor = ctors::_bool;
			RegisterMethod(b._bool, "__nonzero__", methods::self);
			RegisterMethod(b._bool, "__int__", methods::bool_int);
			RegisterMethod(b._bool, "__float__", methods::bool_float);
			RegisterMethod(b._bool, "__str__", methods::bool_str);
			RegisterMethod(b._bool, "__eq__", methods::bool_eq);
			RegisterMethod(b._bool, "__hash__", methods::bool_hash);
			RegisterMethod(b._bool, "__abs__", methods::bool_abs);

			b._false = Alloc(context);
			if (b._false == nullptr)
				throw LibraryInitException();
			b._false->attributes = b._bool->Get<Wg_Obj::Class>().instanceAttributes.Copy();
			b._false->type = "__bool";
			auto falseData = new bool(false);
			Wg_SetUserdata(b._false, falseData);
			Wg_RegisterFinalizer(b._false, [](void* ud) { delete (bool*)ud; }, falseData);

			b._true = Alloc(context);
			if (b._true == nullptr)
				throw LibraryInitException();
			b._true->attributes = b._bool->Get<Wg_Obj::Class>().instanceAttributes.Copy();
			b._true->type = "__bool";
			auto trueData = new bool(true);
			Wg_SetUserdata(b._true, trueData);
			Wg_RegisterFinalizer(b._true, [](void* ud) { delete (bool*)ud; }, trueData);

			b._int = createClass("int");
			RegisterMethod(b._int, "__init__", ctors::_int);
			RegisterMethod(b._int, "__nonzero__", methods::int_nonzero);
			RegisterMethod(b._int, "__int__", methods::self);
			RegisterMethod(b._int, "__float__", methods::int_float);
			RegisterMethod(b._int, "__str__", methods::int_str);
			RegisterMethod(b._int, "__index__", methods::self);
			RegisterMethod(b._int, "__neg__", methods::int_neg);
			RegisterMethod(b._int, "__add__", methods::int_add);
			RegisterMethod(b._int, "__sub__", methods::int_sub);
			RegisterMethod(b._int, "__mul__", methods::int_mul);
			RegisterMethod(b._int, "__truediv__", methods::int_truediv);
			RegisterMethod(b._int, "__floordiv__", methods::int_floordiv);
			RegisterMethod(b._int, "__mod__", methods::int_mod);
			RegisterMethod(b._int, "__pow__", methods::int_pow);
			RegisterMethod(b._int, "__and__", methods::int_and);
			RegisterMethod(b._int, "__or__", methods::int_or);
			RegisterMethod(b._int, "__xor__", methods::int_xor);
			RegisterMethod(b._int, "__invert__", methods::int_invert);
			RegisterMethod(b._int, "__lshift__", methods::int_lshift);
			RegisterMethod(b._int, "__rshift__", methods::int_rshift);
			RegisterMethod(b._int, "__lt__", methods::int_lt);
			RegisterMethod(b._int, "__eq__", methods::int_eq);
			RegisterMethod(b._int, "__hash__", methods::int_hash);
			RegisterMethod(b._int, "__abs__", methods::int_abs);
			RegisterMethod(b._int, "bit_length", methods::int_bit_length);
			RegisterMethod(b._int, "bit_count", methods::int_bit_count);

			b._float = createClass("float");
			RegisterMethod(b._float, "__init__", ctors::_float);
			RegisterMethod(b._float, "__nonzero__", methods::float_nonzero);
			RegisterMethod(b._float, "__int__", methods::float_int);
			RegisterMethod(b._float, "__float__", methods::self);
			RegisterMethod(b._float, "__str__", methods::float_str);
			RegisterMethod(b._float, "__neg__", methods::float_neg);
			RegisterMethod(b._float, "__add__", methods::float_add);
			RegisterMethod(b._float, "__sub__", methods::float_sub);
			RegisterMethod(b._float, "__mul__", methods::float_mul);
			RegisterMethod(b._float, "__truediv__", methods::float_truediv);
			RegisterMethod(b._float, "__floordiv__", methods::float_floordiv);
			RegisterMethod(b._float, "__mod__", methods::float_mod);
			RegisterMethod(b._float, "__pow__", methods::float_pow);
			RegisterMethod(b._float, "__lt__", methods::float_lt);
			RegisterMethod(b._float, "__eq__", methods::float_eq);
			RegisterMethod(b._float, "__hash__", methods::float_hash);
			RegisterMethod(b._float, "__abs__", methods::float_abs);
			RegisterMethod(b._float, "is_integer", methods::float_is_integer);

			b.str = createClass("str");
			RegisterMethod(b.str, "__init__", ctors::str);
			RegisterMethod(b.str, "__nonzero__", methods::str_nonzero);
			RegisterMethod(b.str, "__int__", methods::str_int);
			RegisterMethod(b.str, "__float__", methods::str_float);
			RegisterMethod(b.str, "__str__", methods::self);
			RegisterMethod(b.str, "__repr__", methods::str_repr);
			RegisterMethod(b.str, "__len__", methods::str_len);
			RegisterMethod(b.str, "__add__", methods::str_add);
			RegisterMethod(b.str, "__mul__", methods::str_mul);
			RegisterMethod(b.str, "__getitem__", methods::str_getitem);
			RegisterMethod(b.str, "__contains__", methods::str_contains);
			RegisterMethod(b.str, "__lt__", methods::str_lt);
			RegisterMethod(b.str, "__eq__", methods::str_eq);
			RegisterMethod(b.str, "__hash__", methods::str_hash);
			RegisterMethod(b.str, "capitalize", methods::str_capitalize);
			RegisterMethod(b.str, "casefold", methods::str_casefold);
			RegisterMethod(b.str, "lower", methods::str_lower);
			RegisterMethod(b.str, "upper", methods::str_upper);
			RegisterMethod(b.str, "center", methods::str_center);
			RegisterMethod(b.str, "count", methods::str_count);
			RegisterMethod(b.str, "format", methods::str_format);
			RegisterMethod(b.str, "find", methods::str_find);
			RegisterMethod(b.str, "index", methods::str_index);
			RegisterMethod(b.str, "startswith", methods::str_startswith);
			RegisterMethod(b.str, "endswith", methods::str_endswith);
			RegisterMethod(b.str, "isalnum", methods::str_isalnum);
			RegisterMethod(b.str, "isalpha", methods::str_isalpha);
			RegisterMethod(b.str, "isascii", methods::str_isascii);
			RegisterMethod(b.str, "isdecimal", methods::str_isdecimal);
			RegisterMethod(b.str, "isdigit", methods::str_isdigit);
			RegisterMethod(b.str, "isidentifier", methods::str_isidentifier);
			RegisterMethod(b.str, "islower", methods::str_islower);
			RegisterMethod(b.str, "isupper", methods::str_isupper);
			RegisterMethod(b.str, "isnumeric", methods::str_isnumeric);
			RegisterMethod(b.str, "isprintable", methods::str_isprintable);
			RegisterMethod(b.str, "isspace", methods::str_isspace);
			RegisterMethod(b.str, "join", methods::str_join);
			RegisterMethod(b.str, "ljust", methods::str_ljust);
			RegisterMethod(b.str, "lstrip", methods::str_lstrip);
			RegisterMethod(b.str, "replace", methods::str_replace);
			RegisterMethod(b.str, "rfind", methods::str_rfind);
			RegisterMethod(b.str, "rindex", methods::str_rindex);
			RegisterMethod(b.str, "rjust", methods::str_rjust);
			RegisterMethod(b.str, "rstrip", methods::str_rstrip);
			RegisterMethod(b.str, "split", methods::str_split);
			RegisterMethod(b.str, "splitlines", methods::str_splitlines);
			RegisterMethod(b.str, "strip", methods::str_strip);
			RegisterMethod(b.str, "zfill", methods::str_zfill);

			b.list = createClass("list");
			RegisterMethod(b.list, "__init__", ctors::list);
			RegisterMethod(b.list, "__mul__", methods::collection_mul<Collection::List>);
			RegisterMethod(b.list, "__nonzero__", methods::collection_nonzero<Collection::List>);
			RegisterMethod(b.list, "__str__", methods::collection_str<Collection::List>);
			RegisterMethod(b.list, "__len__", methods::collection_len<Collection::List>);
			RegisterMethod(b.list, "__getitem__", methods::collection_getitem<Collection::List>);
			RegisterMethod(b.list, "__setitem__", methods::list_setitem);
			RegisterMethod(b.list, "__contains__", methods::collection_contains<Collection::List>);
			RegisterMethod(b.list, "__eq__", methods::collection_eq<Collection::List>);
			RegisterMethod(b.list, "__lt__", methods::collection_lt<Collection::List>);
			RegisterMethod(b.list, "count", methods::collection_count<Collection::List>);
			RegisterMethod(b.list, "index", methods::collection_index<Collection::List>);
			RegisterMethod(b.list, "append", methods::list_append);
			RegisterMethod(b.list, "clear", methods::list_clear);
			RegisterMethod(b.list, "copy", methods::list_copy);
			RegisterMethod(b.list, "extend", methods::list_extend);
			RegisterMethod(b.list, "insert", methods::list_insert);
			RegisterMethod(b.list, "pop", methods::list_pop);
			RegisterMethod(b.list, "remove", methods::list_remove);
			RegisterMethod(b.list, "reverse", methods::list_reverse);
			RegisterMethod(b.list, "sort", methods::list_sort);

			b.dict = createClass("dict");
			RegisterMethod(b.dict, "__init__", ctors::map);
			RegisterMethod(b.dict, "__nonzero__", methods::map_nonzero);
			RegisterMethod(b.dict, "__str__", methods::map_str);
			RegisterMethod(b.dict, "__contains__", methods::map_contains);
			RegisterMethod(b.dict, "__getitem__", methods::map_getitem);
			RegisterMethod(b.dict, "__iter__", methods::map_iter);
			RegisterMethod(b.dict, "__len__", methods::map_len);
			RegisterMethod(b.dict, "__setitem__", methods::map_setitem);
			RegisterMethod(b.dict, "clear", methods::map_clear);
			RegisterMethod(b.dict, "copy", methods::map_copy);
			RegisterMethod(b.dict, "get", methods::map_get);
			RegisterMethod(b.dict, "keys", methods::map_iter);
			RegisterMethod(b.dict, "values", methods::map_values);
			RegisterMethod(b.dict, "items", methods::map_items);
			RegisterMethod(b.dict, "pop", methods::map_pop);
			RegisterMethod(b.dict, "popitem", methods::map_popitem);
			RegisterMethod(b.dict, "setdefault", methods::map_setdefault);
			RegisterMethod(b.dict, "update", methods::map_update);

			b.set = createClass("set");
			RegisterMethod(b.set, "__init__", ctors::set);
			RegisterMethod(b.set, "__nonzero__", methods::set_nonzero);
			RegisterMethod(b.set, "__str__", methods::set_str);
			RegisterMethod(b.set, "__contains__", methods::set_contains);
			RegisterMethod(b.set, "__iter__", methods::set_iter);
			RegisterMethod(b.set, "__len__", methods::set_len);
			RegisterMethod(b.set, "add", methods::set_add);
			RegisterMethod(b.set, "clear", methods::set_clear);
			RegisterMethod(b.set, "copy", methods::set_copy);
			RegisterMethod(b.set, "difference", methods::set_difference);
			RegisterMethod(b.set, "discard", methods::set_discard);
			RegisterMethod(b.set, "intersection", methods::set_intersection);
			RegisterMethod(b.set, "isdisjoint", methods::set_isdisjoint);
			RegisterMethod(b.set, "issubset", methods::set_issubset);
			RegisterMethod(b.set, "issuperset", methods::set_issuperset);
			RegisterMethod(b.set, "pop", methods::set_pop);
			RegisterMethod(b.set, "remove", methods::set_remove);
			RegisterMethod(b.set, "symmetric_difference", methods::set_symmetric_difference);
			RegisterMethod(b.set, "union", methods::set_union);
			RegisterMethod(b.set, "update", methods::set_update);

			b.dictKeysIter = createClass("__DictKeysIter", nullptr, false);
			RegisterMethod(b.dictKeysIter, "__init__", ctors::DictIter);
			RegisterMethod(b.dictKeysIter, "__next__", methods::DictKeysIter_next);
			RegisterMethod(b.dictKeysIter, "__iter__", methods::self);

			b.dictValuesIter = createClass("__DictValuesIter", nullptr, false);
			RegisterMethod(b.dictValuesIter, "__init__", ctors::DictIter);
			RegisterMethod(b.dictValuesIter, "__next__", methods::DictValuesIter_next);
			RegisterMethod(b.dictValuesIter, "__iter__", methods::self);

			b.dictItemsIter = createClass("__DictItemsIter", nullptr, false);
			RegisterMethod(b.dictItemsIter, "__init__", ctors::DictIter);
			RegisterMethod(b.dictItemsIter, "__next__", methods::DictItemsIter_next);
			RegisterMethod(b.dictItemsIter, "__iter__", methods::self);

			b.setIter = createClass("__SetIter", nullptr, false);
			RegisterMethod(b.setIter, "__init__", ctors::SetIter);
			RegisterMethod(b.setIter, "__next__", methods::SetIter_next);
			RegisterMethod(b.setIter, "__iter__", methods::self);

			b.file = createClass("__File", nullptr, false);
			RegisterMethod(b.file, "__init__", ctors::File);
			RegisterMethod(b.file, "__iter__", methods::File_iter);
			RegisterMethod(b.file, "__enter__", methods::self);
			RegisterMethod(b.file, "__exit__", methods::File_exit);
			RegisterMethod(b.file, "close", methods::File_close);
			RegisterMethod(b.file, "read", methods::File_read);
			RegisterMethod(b.file, "readline", methods::File_readline);
			RegisterMethod(b.file, "readlines", methods::File_readlines);
			RegisterMethod(b.file, "write", methods::File_write);
			RegisterMethod(b.file, "writelines", methods::File_writelines);
			RegisterMethod(b.file, "readable", methods::File_readable);
			RegisterMethod(b.file, "writable", methods::File_writable);
			RegisterMethod(b.file, "seekable", methods::File_seekable);
			RegisterMethod(b.file, "seek", methods::File_seek);
			RegisterMethod(b.file, "tell", methods::File_tell);
			if (context->config.enableOSAccess)
				Wg_SetGlobal(context, "open", b.file);

			// Add native free functions
			b.isinstance = RegisterFunction(context, "isinstance", lib::isinstance);
			RegisterFunction(context, "bin", lib::base_str<2>);
			RegisterFunction(context, "oct", lib::base_str<8>);
			RegisterFunction(context, "hex", lib::base_str<16>);
			RegisterFunction(context, "callable", lib::callable);
			RegisterFunction(context, "chr", lib::chr);
			RegisterFunction(context, "compile", lib::compile);
			RegisterFunction(context, "eval", lib::eval);
			RegisterFunction(context, "exec", lib::exec);
			RegisterFunction(context, "getattr", lib::getattr);
			RegisterFunction(context, "id", lib::id);
			RegisterFunction(context, "input", lib::input);
			RegisterFunction(context, "ord", lib::ord);
			RegisterFunction(context, "pow", lib::pow);
			RegisterFunction(context, "print", lib::print);
			RegisterFunction(context, "round", lib::round);
			RegisterFunction(context, "setattr", lib::setattr);
			RegisterFunction(context, "exit", lib::exit);
			RegisterFunction(context, "quit", lib::exit);
			
			// Initialize the rest with a script
			if (Execute(context, BUILTINS_CODE, "__builtins__") == nullptr)
				throw LibraryInitException();

			b.len = getGlobal("len");
			b.repr = getGlobal("repr");
			b.hash = getGlobal("hash");
			b.slice = getGlobal("slice");
			b.defaultIter = getGlobal("__DefaultIter");
			b.defaultReverseIter = getGlobal("__DefaultReverseIter");
			b.codeObject = getGlobal("__CodeObject");
			b.moduleObject = createClass("ModuleObject", nullptr, false);
			b.readlineIter = getGlobal("__ReadLineIter");
			
			b.baseException = getGlobal("BaseException");
			b.systemExit = getGlobal("SystemExit");
			b.exception = getGlobal("Exception");
			b.stopIteration = getGlobal("StopIteration");
			b.arithmeticError = getGlobal("ArithmeticError");
			b.overflowError = getGlobal("OverflowError");
			b.zeroDivisionError = getGlobal("ZeroDivisionError");
			b.attributeError = getGlobal("AttributeError");
			b.importError = getGlobal("ImportError");
			b.syntaxError = getGlobal("SyntaxError");
			b.lookupError = getGlobal("LookupError");
			b.indexError = getGlobal("IndexError");
			b.keyError = getGlobal("KeyError");
			b.memoryError = getGlobal("MemoryError");
			b.nameError = getGlobal("NameError");
			b.osError = getGlobal("OSError");
			b.isADirectoryError = getGlobal("IsADirectoryError");
			b.runtimeError = getGlobal("RuntimeError");
			b.notImplementedError = getGlobal("NotImplementedError");
			b.recursionError = getGlobal("RecursionError");
			b.typeError = getGlobal("TypeError");
			b.valueError = getGlobal("ValueError");

			b.memoryErrorInstance = Wg_Call(b.memoryError, nullptr, 0);
			if (b.memoryErrorInstance == nullptr)
				throw LibraryInitException();

			b.recursionErrorInstance = Wg_Call(b.recursionError, nullptr, 0);
			if (b.recursionErrorInstance == nullptr)
				throw LibraryInitException();
			
		} catch (LibraryInitException&) {
			std::abort(); // Internal error
		}
		return true;
	}
} // namespace wings


#include <vector>
#include <string>

namespace wings {
	struct Token {
		enum class Type {
			Null,
			Bool,
			Int,
			Float,
			String,
			Symbol,
			Word,
			Keyword,
		} type;

		std::string text;
		SourcePosition srcPos;

		struct {
			union {
				bool b;
				Wg_int i;
				Wg_float f;
			};
			std::string s;
		} literal;

		std::string ToString() const;
	};

	struct LexTree {
		std::vector<Token> tokens;
		std::vector<LexTree> children;
	};

	struct LexResult {
		std::vector<std::string> originalSource;
		LexTree lexTree; // Root tree contains no tokens
		CodeError error;
	};

	LexResult Lex(std::string code);
}


#include <vector>
#include <optional>
#include <unordered_set>

namespace wings {

	enum class Operation {
		Literal, Variable,
		Tuple, List, Map, Set,
		ListComprehension,
		Index, Call, Slice,
		Pos, Neg,
		Add, Sub, Mul, Div, IDiv, Mod, Pow,
		Eq, Ne, Lt, Le, Gt, Ge,
		And, Or, Not,
		In, NotIn, Is, IsNot,
		BitAnd, BitOr, BitNot, BitXor,
		ShiftL, ShiftR,
		IfElse,
		Assign, AddAssign, SubAssign, MulAssign,
		DivAssign, IDivAssign, ModAssign, PowAssign,
		AndAssign, OrAssign, XorAssign,
		ShiftLAssign, ShiftRAssign,
		Dot,
		Function,
		Unpack, UnpackMapForMapCreation, UnpackMapForCall,
		Kwarg,

		CompoundAssignment,
	};

	enum class AssignType {
		None,
		// var = value
		Direct,
		// var[index] = value
		Index,
		// var.member = value
		Member,
		// (x, y) = (a, b)
		Pack,
	};

	struct AssignTarget {
		AssignType type{}; // Either Direct or Pack
		std::string direct;
		std::vector<AssignTarget> pack;
	};

	struct LiteralValue {
		enum class Type {
			Null,
			Bool,
			Int,
			Float,
			String,
		} type;

		union {
			bool b;
			Wg_int i;
			Wg_float f;
		};
		std::string s;
	};

	struct Statement;
	struct Parameter;

	struct Expression {
		Operation operation{};
		std::vector<Expression> children;
		SourcePosition srcPos;

		AssignTarget assignTarget;
		std::string variableName;
		LiteralValue literalValue;
		struct {
			std::string name;
			mutable std::vector<Parameter> parameters;
			std::unordered_set<std::string> globalCaptures;
			std::unordered_set<std::string> localCaptures;
			std::unordered_set<std::string> variables;
			std::vector<Statement> body;
		} def;

		struct {
			std::string listName;
			std::vector<Statement> forBody;
		} listComp;

		Expression() = default;
		Expression(Expression&&) = default;
		Expression& operator=(Expression&&) = default;
	};

	struct Parameter {
		std::string name;
		std::optional<Expression> defaultValue;
		enum class Type { Named, ListArgs, Kwargs } type = Type::Named;
	};

	struct TokenIter {
		TokenIter(const std::vector<Token>& tokens);
		TokenIter& operator++();
		TokenIter& operator--();
		const Token& operator*() const;
		const Token* operator->() const;
		bool operator==(const TokenIter& rhs) const;
		bool operator!=(const TokenIter& rhs) const;
		bool EndReached() const;
	private:
		size_t index;
		const std::vector<Token>* tokens;
	};

	CodeError ParseExpression(TokenIter& p, Expression& out, bool disableInOp = false);
	CodeError ParseExpressionList(TokenIter& p, const std::string& terminate, std::vector<Expression>& out, bool isFnCall = false, bool* seenComma = nullptr);
	bool IsAssignableExpression(const Expression& expr, AssignTarget& target, bool onlyDirectOrPack = false);

}


#include <string>
#include <vector>
#include <optional>
#include <unordered_set>
#include <memory>

namespace wings {

	struct Statement {
		enum class Type {
			Root,
			Pass,
			Expr,
			Nonlocal, Global,
			Def, Class, Return,
			If, Elif, Else,
			While, For,
			Try, Except, Finally, Raise,
			Break, Continue,
			Composite,
			Import, ImportFrom,
		} type;

		SourcePosition srcPos;
		Expression expr;
		std::vector<Statement> body;
		std::unique_ptr<Statement> elseClause;

		struct {
			AssignTarget assignTarget;
		} forLoop;
		struct {
			std::string name;
		} capture;
		struct {
			std::string name;
			std::vector<std::string> methodNames;
			std::vector<Expression> bases;
		} klass;
		struct {
			std::vector<Statement> exceptClauses;
			std::vector<Statement> finallyClause;
		} tryBlock;
		struct {
			std::string var;
			std::optional<Expression> exceptType;
		} exceptBlock;
		struct {
			std::string module;
			std::string alias;
		} import;
		struct {
			std::string module;
			std::vector<std::string> names;
			std::string alias;
		} importFrom;
	};

	struct ParseResult {
		CodeError error;
		Statement parseTree; // Root is treated similar to a def
	};

	ParseResult Parse(const LexTree& lexTree);

	std::unordered_set<std::string> GetReferencedVariables(const Expression& expr);
	CodeError ParseParameterList(TokenIter& p, std::vector<Parameter>& out);
	CodeError ParseForLoopVariableList(TokenIter& p, std::vector<std::string>& vars, bool& isTuple);
	Statement TransformForToWhile(Statement forLoop);
	void ExpandCompositeStatements(std::vector<Statement>& statements);

}


#include <string>
#include <vector>
#include <variant>
#include <memory>

namespace wings {
	struct Instruction;

	struct DefInstruction {
		size_t defaultParameterCount{};
		std::string prettyName;
		bool isMethod = false;
		std::vector<Parameter> parameters;
		std::vector<std::string> globalCaptures;
		std::vector<std::string> localCaptures;
		std::vector<std::string> variables;
		RcPtr<std::vector<Instruction>> instructions;
		std::optional<std::string> listArgs;
		std::optional<std::string> kwArgs;
	};

	struct ClassInstruction {
		std::vector<std::string> methodNames;
		std::string prettyName;
	};

	using LiteralInstruction = std::variant<std::nullptr_t, bool, Wg_int, Wg_float, std::string>;

	struct StringArgInstruction {
		std::string string;
	};

	struct JumpInstruction {
		size_t location;
	};

	struct DirectAssignInstruction {
		AssignTarget assignTarget;
	};

	struct TryFrameInstruction {
		size_t exceptJump;
		size_t finallyJump;
	};

	struct ImportInstruction {
		std::string module;
		std::string alias;
	};

	struct ImportFromInstruction {
		std::string module;
		std::vector<std::string> names;
		std::string alias;
	};

	struct Instruction {
		enum class Type {
			Literal,
			Tuple, List, Map, Set,
			Slice,
			Def,
			Class,
			Variable,
			Dot,
			Import,
			ImportFrom,
			Operation,
			Pop,
			Not,
			Is,

			DirectAssign,
			MemberAssign,

			Jump,
			JumpIfFalsePop,
			JumpIfFalse,
			JumpIfTrue,
			Return,

			Raise,
			PushTry,
			PopTry,
			Except,
			CurrentException,
			IsInstance,

			Call,
			PushArgFrame,
			Unpack,
			UnpackMapForMapCreation,
			UnpackMapForCall,
			PushKwarg,
		} type{};

		std::unique_ptr<DirectAssignInstruction> directAssign;
		std::unique_ptr<LiteralInstruction> literal;
		std::unique_ptr<StringArgInstruction> string;
		std::unique_ptr<DefInstruction> def;
		std::unique_ptr<ClassInstruction> klass;
		std::unique_ptr<JumpInstruction> jump;
		std::unique_ptr<TryFrameInstruction> pushTry;
		std::unique_ptr<ImportInstruction> import;
		std::unique_ptr<ImportFromInstruction> importFrom;

		SourcePosition srcPos;
	};

	std::vector<Instruction> Compile(const Statement& parseTree);
}


#include <vector>
#include <string>
#include <unordered_map>
#include <optional>
#include <stack>

namespace wings {

	struct DefObject {
		static Wg_Obj* Run(Wg_Context* context, Wg_Obj** args, int argc);
		Wg_Context* context{};
		RcPtr<std::vector<Instruction>> instructions;
		std::string module;
		std::string prettyName;
		std::vector<std::string> localVariables;
		std::vector<std::string> parameterNames;
		std::vector<Wg_Obj*> defaultParameterValues;
		std::optional<std::string> listArgs;
		std::optional<std::string> kwArgs;
		std::unordered_map<std::string, RcPtr<Wg_Obj*>> captures;
		RcPtr<std::vector<std::string>> originalSource;
	};

	struct TryFrame {
		size_t exceptJump;
		size_t finallyJump;
		bool isHandlingException;
		size_t stackSize;
	};

	struct Executor {
		Wg_Obj* Run();

		void GetReferences(std::deque<const Wg_Obj*>& refs);

		void PushStack(Wg_Obj* obj);
		Wg_Obj* PopStack();
		void PopStackUntil(size_t size);
		Wg_Obj* PeekStack();
		void ClearStack();
		size_t PopArgFrame();

		void DoInstruction(const Instruction& instr);

		Wg_Obj* GetVariable(const std::string& name);
		void SetVariable(const std::string& name, Wg_Obj* value);

		Wg_Obj* DirectAssign(const AssignTarget& target, Wg_Obj* value);

		DefObject* def;
		Wg_Context* context;
		size_t pc{};
		std::vector<Wg_Obj*> stack;
		std::stack<size_t> argFrames;
		std::vector<std::vector<Wg_Obj*>> kwargsStack;
		std::unordered_map<std::string, RcPtr<Wg_Obj*>> variables;
		std::optional<Wg_Obj*> exitValue;

		std::stack<TryFrame> tryFrames;
	};

}


#include <algorithm>
#include <unordered_set>

namespace wings {

	std::atomic<Wg_ErrorCallback> errorCallback;
	
	Wg_Obj* Alloc(Wg_Context* context) {
		// Objects should never be allocated while the garbage collector is running.
		WG_ASSERT(!context->gcRunning);
		
		// Check allocation limits
		if (context->mem.size() >= (size_t)context->config.maxAlloc) {
			// Too many objects. Try to free up objects
			Wg_CollectGarbage(context);
			if (context->mem.size() >= (size_t)context->config.maxAlloc) {
				// If there are still too many objects then set a MemoryException
				Wg_RaiseException(context, WG_EXC_MEMORYERROR);
				return nullptr;
			}
		}

		// Check if GC should run
		size_t threshold = (size_t)((double)context->config.gcRunFactor * context->lastObjectCountAfterGC);
		if (context->mem.size() >= threshold) {
			Wg_CollectGarbage(context);
		}

		// Allocate new object
		auto obj = std::make_unique<Wg_Obj>();
		obj->context = context;

		auto p = obj.get();
		context->mem.push_back(std::move(obj));
		return p;
	}

	void CallErrorCallback(const char* message) {
		Wg_ErrorCallback cb = errorCallback;

		if (cb) {
			cb(message);
		} else {
			std::abort();
		}
	}

	size_t Guid() {
		static std::atomic_size_t i = 0;
		return ++i;
	}

	std::string WObjTypeToString(const Wg_Obj* obj) {
		if (Wg_IsNone(obj)) {
			return "NoneType";
		} else if (Wg_IsBool(obj)) {
			return "bool";
		} else if (Wg_IsInt(obj)) {
			return "int";
		} else if (Wg_IsIntOrFloat(obj)) {
			return "float";
		} else if (Wg_IsString(obj)) {
			return "str";
		} else if (Wg_IsTuple(obj)) {
			return "tuple";
		} else if (Wg_IsList(obj)) {
			return "list";
		} else if (Wg_IsDictionary(obj)) {
			return "dict";
		} else if (Wg_IsSet(obj)) {
			return "set";
		} else if (Wg_IsFunction(obj)) {
			return "function";
		} else if (Wg_IsClass(obj)) {
			return "class";
		} else if (obj->type == "__object") {
			return "object";
		} else {
			return obj->type;
		}
	}

	std::string CodeError::ToString() const {
		if (good) {
			return "Success";
		} else {
			return '(' + std::to_string(srcPos.line + 1) + ','
				+ std::to_string(srcPos.column + 1) + ") "
				+ message;
		}
	}

	CodeError::operator bool() const {
		return !good;
	}

	CodeError CodeError::Good() {
		return CodeError{ true, {}, {} };
	}

	CodeError CodeError::Bad(std::string message, SourcePosition srcPos) {
		return CodeError{
			.good = false,
			.srcPos = srcPos,
			.message = message
		};
	}

	size_t WObjHasher::operator()(Wg_Obj* obj) const {
		if (Wg_Obj* hash = Wg_UnaryOp(WG_UOP_HASH, obj))
			return (size_t)Wg_GetInt(hash);
		throw HashException();
	}

	bool WObjComparer::operator()(Wg_Obj* lhs, Wg_Obj* rhs) const {
		if (Wg_Obj* eq = Wg_BinaryOp(WG_BOP_EQ, lhs, rhs))
			return Wg_GetBool(eq);
		throw HashException();
	}

	static const std::unordered_set<std::string_view> RESERVED = {
		"True", "False", "None",
		"and", "or", "not",
		"if", "else", "elif", "while", "for",
		"class", "def",
		"try", "except", "finally", "raise", "with", "assert",
		"return", "break", "continue", "pass",
		"global", "nonlocal", "del",
		"from", "import",
		"lambda", "in", "as", "is",
		"await", "async", "yield",
	};

	bool IsKeyword(std::string_view s) {
		return RESERVED.contains(s);
	}

	bool IsValidIdentifier(std::string_view s) {
		if (s.empty())
			return false;

		auto isalpha = [](char c) {
			return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
		};

		auto isalnum = [isalpha](char c) {
			return isalpha(c) || (c >= '0' && c <= '9');
		};
		
		return isalpha(s[0])
			&& std::all_of(s.begin() + 1, s.end(), isalnum)
			&& !IsKeyword(s);
	}
	
	void RegisterMethod(Wg_Obj* klass, const char* name, Wg_Function fptr) {
		if (Wg_IsClass(klass)) {
			if (Wg_BindMethod(klass, name, fptr, nullptr) == nullptr)
				throw LibraryInitException();
		} else {
			Wg_Obj* method = Wg_NewFunction(klass->context, fptr, nullptr, name);
			if (method == nullptr)
				throw LibraryInitException();
			method->Get<Wg_Obj::Func>().isMethod = true;
			Wg_SetAttribute(klass, name, method);
		}
	}

	Wg_Obj* RegisterFunction(Wg_Context* context, const char* name, Wg_Function fptr) {
		Wg_Obj* obj = Wg_NewFunction(context, fptr, nullptr, name);
		if (obj == nullptr)
			throw LibraryInitException();
		Wg_SetGlobal(context, name, obj);
		return obj;
	}

	void AddAttributeToClass(Wg_Obj* klass, const char* attribute, Wg_Obj* value) {
		WG_ASSERT_VOID(klass && attribute && value && Wg_IsClass(klass) && IsValidIdentifier(attribute));
		klass->Get<Wg_Obj::Class>().instanceAttributes.Set(attribute, value);
	}

	Wg_Obj* Compile(Wg_Context* context, const char* code, const char* module, const char* prettyName, bool expr) {
		WG_ASSERT(context && code);

		if (prettyName == nullptr)
			prettyName = DEFAULT_FUNC_NAME;

		auto lexResult = Lex(code);
		auto originalSource = MakeRcPtr<std::vector<std::string>>(lexResult.originalSource);

		auto raiseException = [&](const CodeError& error) {
			std::string_view lineText;
			if (error.srcPos.line < originalSource->size()) {
				lineText = (*originalSource)[error.srcPos.line];
			}
			context->currentTrace.push_back(TraceFrame{
				error.srcPos,
				lineText,
				module,
				prettyName,
				true
				});

			Wg_RaiseException(context, WG_EXC_SYNTAXERROR, error.message.c_str());

			context->currentTrace.pop_back();
		};

		if (lexResult.error) {
			raiseException(lexResult.error);
			return nullptr;
		}

		auto parseResult = Parse(lexResult.lexTree);
		if (parseResult.error) {
			raiseException(parseResult.error);
			return nullptr;
		}

		if (expr) {
			std::vector<Statement> body = std::move(parseResult.parseTree.expr.def.body);
			if (body.size() != 1 || body[0].type != Statement::Type::Expr) {
				raiseException(CodeError::Bad("Invalid syntax"));
				return nullptr;
			}

			Statement ret{};
			ret.srcPos = body[0].srcPos;
			ret.type = Statement::Type::Return;
			ret.expr = std::move(body[0].expr);

			parseResult.parseTree.expr.def.body.clear();
			parseResult.parseTree.expr.def.body.push_back(std::move(ret));
		}

		auto* def = new DefObject();
		def->context = context;
		def->module = module;
		def->prettyName = prettyName;
		def->originalSource = std::move(originalSource);
		auto instructions = Compile(parseResult.parseTree);
		def->instructions = MakeRcPtr<std::vector<Instruction>>(std::move(instructions));

		Wg_Obj* obj = Wg_NewFunction(context, &DefObject::Run, def);
		if (obj == nullptr) {
			delete def;
			return nullptr;
		}
		
		Wg_RegisterFinalizer(obj, [](void* ud) { delete (DefObject*)ud; }, def);

		return obj;
	}

	Wg_Obj* Execute(Wg_Context* context, const char* code, const char* module) {
		if (Wg_Obj* fn = Compile(context, code, module, module, false)) {
			return Wg_Call(fn, nullptr, 0);
		} else {
			return nullptr;
		}
	}

	Rng::Rng() :
		engine(std::random_device()())
	{
	}

	void Rng::Seed(Wg_int seed) {
		engine.seed((unsigned long long)seed);
		dist.reset();
	}

	Wg_float Rng::Rand() {
		return dist(engine);
	}

	Wg_int Rng::Int(Wg_int minIncl, Wg_int maxIncl) {
		auto i = (Wg_int)((maxIncl - minIncl + 1) * Rand() + minIncl);

		if (i > maxIncl) // Just in case
			return maxIncl;

		return i;
	}

	Wg_float Rng::Float(Wg_float minIncl, Wg_float maxIncl) {
		return (maxIncl - minIncl) * Rand() + minIncl;
	}

	std::mt19937_64& Rng::Engine() {
		return engine;
	}
	
	bool InitArgv(Wg_Context* context, const char* const* argv, int argc) {
		Wg_Obj* list = Wg_NewList(context);
		if (list == nullptr)
			return false;

		const char* empty = "";
		if (argc == 0) {
			argv = &empty;
			argc = 1;
		}
		
		for (int i = 0; i < argc; i++) {
			Wg_Obj* str = Wg_NewString(context, argv[i]);
			if (str == nullptr)
				return false;
			if (Wg_CallMethod(list, "append", &str, 1) == nullptr)
				return false;
		}
		
		context->argv = list;
		return true;
	}
}


#include <unordered_map>

namespace wings {

	static thread_local std::vector<size_t> breakInstructions;
	static thread_local std::vector<size_t> continueInstructions;

	static void CompileBody(const std::vector<Statement>& body, std::vector<Instruction>& instructions);
	static void CompileExpression(const Expression& expression, std::vector<Instruction>& instructions);
	static void CompileFunction(const Expression& node, std::vector<Instruction>& instructions);

	static const std::unordered_map<Operation, std::string> OP_METHODS = {
		{ Operation::Index,  "__getitem__"  },
		{ Operation::Pos,	 "__pos__"      },
		{ Operation::Neg,	 "__neg__"      },
		{ Operation::Add,	 "__add__"      },
		{ Operation::Sub,	 "__sub__"      },
		{ Operation::Mul,	 "__mul__"      },
		{ Operation::Div,	 "__truediv__"  },
		{ Operation::IDiv,	 "__floordiv__" },
		{ Operation::Mod,	 "__mod__"      },
		{ Operation::Pow,	 "__pow__"      },
		{ Operation::Eq,	 "__eq__"       },
		{ Operation::Ne,	 "__ne__"       },
		{ Operation::Lt,	 "__lt__"       },
		{ Operation::Le,	 "__le__"       },
		{ Operation::Gt,	 "__gt__"       },
		{ Operation::Ge,	 "__ge__"       },
		{ Operation::In,	 "__contains__" },
		{ Operation::BitAnd, "__and__"      },
		{ Operation::BitOr,  "__or__"       },
		{ Operation::BitNot, "__invert__"   },
		{ Operation::BitXor, "__xor__"      },
		{ Operation::ShiftL, "__lshift__"   },
		{ Operation::ShiftR, "__rshift__"   },

		{ Operation::AddAssign, "__iadd__"       },
		{ Operation::SubAssign, "__isub__"       },
		{ Operation::MulAssign, "__imul__"       },
		{ Operation::DivAssign, "__itruediv__"   },
		{ Operation::IDivAssign, "__ifloordiv__" },
		{ Operation::ModAssign, "__imod__"       },
		{ Operation::PowAssign, "__ipow__"       },
		{ Operation::AndAssign, "__iand__"      },
		{ Operation::OrAssign, "__ior__"         },
		{ Operation::XorAssign, "__ixor__"       },
		{ Operation::ShiftLAssign, "__ilshift__" },
		{ Operation::ShiftRAssign, "__irshift__" },
	};

	static const std::unordered_set<Operation> COMPOUND_OPS = {
		Operation::AddAssign,
		Operation::SubAssign,
		Operation::MulAssign,
		Operation::PowAssign,
		Operation::DivAssign,
		Operation::IDivAssign,
		Operation::ModAssign,
		Operation::ShiftLAssign,
		Operation::ShiftRAssign,
		Operation::OrAssign,
		Operation::AndAssign,
		Operation::XorAssign,
	};

	static void CompileInlineIfElse(const Expression& expression, std::vector<Instruction>& instructions) {
		const auto& condition = expression.children[0];
		const auto& trueCase = expression.children[1];
		const auto& falseCase = expression.children[2];
		
		CompileExpression(condition, instructions);

		Instruction falseJump{};
		falseJump.srcPos = condition.srcPos;
		falseJump.type = Instruction::Type::JumpIfFalsePop;
		falseJump.jump = std::make_unique<JumpInstruction>();
		size_t falseJumpIndex = instructions.size();
		instructions.push_back(std::move(falseJump));

		CompileExpression(trueCase, instructions);

		Instruction trueJump{};
		trueJump.srcPos = condition.srcPos;
		trueJump.type = Instruction::Type::Jump;
		trueJump.jump = std::make_unique<JumpInstruction>();
		size_t trueJumpIndex = instructions.size();
		instructions.push_back(std::move(trueJump));

		instructions[falseJumpIndex].jump->location = instructions.size();

		CompileExpression(falseCase, instructions);

		instructions[trueJumpIndex].jump->location = instructions.size();
	}

	static void CompileShortcircuitLogical(const Expression& expr, std::vector<Instruction>& instructions) {
		const auto& lhs = expr.children[0];
		const auto& rhs = expr.children[1];

		CompileExpression(lhs, instructions);
		
		size_t jmpInstrIndex = instructions.size();
		Instruction jmp{};
		if (expr.operation == Operation::And) {
			jmp.type = Instruction::Type::JumpIfFalse;
		} else {
			jmp.type = Instruction::Type::JumpIfTrue;
		}
		jmp.srcPos = expr.srcPos;
		jmp.jump = std::make_unique<JumpInstruction>();
		instructions.push_back(std::move(jmp));

		CompileExpression(rhs, instructions);
		
		instructions[jmpInstrIndex].jump->location = instructions.size();
	}

	static void CompileIn(const Expression& expression, std::vector<Instruction>& instructions) {
		Instruction argFrame{};
		argFrame.srcPos = expression.srcPos;
		argFrame.type = Instruction::Type::PushArgFrame;
		instructions.push_back(std::move(argFrame));

		CompileExpression(expression.children[1], instructions);

		Instruction dot{};
		dot.srcPos = expression.srcPos;
		dot.type = Instruction::Type::Dot;
		dot.string = std::make_unique<StringArgInstruction>();
		dot.string->string = "__contains__";
		instructions.push_back(std::move(dot));

		CompileExpression(expression.children[0], instructions);

		Instruction call{};
		call.srcPos = expression.srcPos;
		call.type = Instruction::Type::Call;
		instructions.push_back(std::move(call));

		if (expression.operation == Operation::NotIn) {
			Instruction notInstr{};
			notInstr.srcPos = expression.srcPos;
			notInstr.type = Instruction::Type::Not;
			instructions.push_back(std::move(notInstr));
		}
	}

	static void CompileAssignment(
		const AssignTarget& assignTarget,
		const Expression& assignee,
		const Expression& value,
		const SourcePosition& srcPos,
		std::vector<Instruction>& instructions) {

		Instruction instr{};
		instr.srcPos = srcPos;

		switch (assignTarget.type) {
		case AssignType::Direct:
		case AssignType::Pack:
			// <assign>
			//		<assignee>
			//		<expr>
			CompileExpression(value, instructions);
			instr.directAssign = std::make_unique<DirectAssignInstruction>();
			instr.directAssign->assignTarget = assignTarget;
			instr.type = Instruction::Type::DirectAssign;
			break;
		case AssignType::Index: {
			// <assign>
			//		<assignee>
			//			<var>
			//			<index>
			//		<expr>
			Instruction argFrame{};
			argFrame.srcPos = srcPos;
			argFrame.type = Instruction::Type::PushArgFrame;
			instructions.push_back(std::move(argFrame));

			CompileExpression(assignee.children[0], instructions);

			Instruction dot{};
			dot.srcPos = srcPos;
			dot.type = Instruction::Type::Dot;
			dot.string = std::make_unique<StringArgInstruction>();
			dot.string->string = "__setitem__";
			instructions.push_back(std::move(dot));

			CompileExpression(assignee.children[1], instructions);
			CompileExpression(value, instructions);

			instr.type = Instruction::Type::Call;
			break;
		}
		case AssignType::Member:
			// <assign>
			//		<assignee>
			//			<var>
			//		<expr>
			CompileExpression(assignee.children[0], instructions);
			CompileExpression(value, instructions);
			instr.string = std::make_unique<StringArgInstruction>();
			instr.string->string = assignee.variableName;
			instr.type = Instruction::Type::MemberAssign;
			break;
		default:
			WG_UNREACHABLE();
		}

		instructions.push_back(std::move(instr));
	}

	static void CompileExpression(const Expression& expression, std::vector<Instruction>& instructions) {
		if (expression.operation == Operation::Assign) {
			CompileAssignment(expression.assignTarget, expression.children[0], expression.children[1], expression.srcPos, instructions);
			return;
		}

		auto compileChildExpressions = [&] {
			for (size_t i = 0; i < expression.children.size(); i++)
				CompileExpression(expression.children[i], instructions);
		};

		Instruction instr{};
		instr.srcPos = expression.srcPos;

		switch (expression.operation) {
		case Operation::Literal:
			instr.literal = std::make_unique<LiteralInstruction>();
			switch (expression.literalValue.type) {
			case LiteralValue::Type::Null: *instr.literal = nullptr; break;
			case LiteralValue::Type::Bool: *instr.literal = expression.literalValue.b; break;
			case LiteralValue::Type::Int: *instr.literal = expression.literalValue.i; break;
			case LiteralValue::Type::Float: *instr.literal = expression.literalValue.f; break;
			case LiteralValue::Type::String: *instr.literal = expression.literalValue.s; break;
			default: WG_UNREACHABLE();
			}
			instr.type = Instruction::Type::Literal;
			break;
		case Operation::Tuple:
		case Operation::List:
		case Operation::Map:
		case Operation::Set: {
			Instruction argFrame{};
			argFrame.srcPos = expression.srcPos;
			argFrame.type = Instruction::Type::PushArgFrame;
			instructions.push_back(std::move(argFrame));

			compileChildExpressions();

			switch (expression.operation) {
			case Operation::Tuple: instr.type = Instruction::Type::Tuple; break;
			case Operation::List: instr.type = Instruction::Type::List; break;
			case Operation::Map: instr.type = Instruction::Type::Map; break;
			case Operation::Set: instr.type = Instruction::Type::Set; break;
			default: WG_UNREACHABLE();
			}
			break;
		}
		case Operation::Variable:
			instr.string = std::make_unique<StringArgInstruction>();
			instr.string->string = expression.variableName;
			instr.type = Instruction::Type::Variable;
			break;
		case Operation::Dot:
			compileChildExpressions();
			instr.string = std::make_unique<StringArgInstruction>();
			instr.string->string = expression.variableName;
			instr.type = Instruction::Type::Dot;
			break;
		case Operation::Call: {
			Instruction pushArgFrame{};
			pushArgFrame.srcPos = expression.srcPos;
			pushArgFrame.type = Instruction::Type::PushArgFrame;
			instructions.push_back(std::move(pushArgFrame));

			compileChildExpressions();
			instr.type = Instruction::Type::Call;
			break;
		}
		case Operation::Or:
		case Operation::And:
			CompileShortcircuitLogical(expression, instructions);
			return;
		case Operation::Not:
			CompileExpression(expression.children[0], instructions);
			instr.type = Instruction::Type::Not;
			break;
		case Operation::In:
		case Operation::NotIn:
			CompileIn(expression, instructions);
			return;
		case Operation::Is:
		case Operation::IsNot:
			compileChildExpressions();
			
			instr.type = Instruction::Type::Is;
			instructions.push_back(std::move(instr));
			
			if (expression.operation == Operation::IsNot) {
				Instruction notInstr{};
				notInstr.srcPos = expression.srcPos;
				notInstr.type = Instruction::Type::Not;
				instructions.push_back(std::move(notInstr));
			}
			return;
		case Operation::IfElse:
			CompileInlineIfElse(expression, instructions);
			return;
		case Operation::Unpack:
			compileChildExpressions();
			instr.type = Instruction::Type::Unpack;
			break;
		case Operation::UnpackMapForMapCreation:
			compileChildExpressions();
			instr.type = Instruction::Type::UnpackMapForMapCreation;
			break;
		case Operation::UnpackMapForCall:
			compileChildExpressions();
			instr.type = Instruction::Type::UnpackMapForCall;
			break;
		case Operation::Slice: {
			// var.__getitem__(slice(...))
			Instruction argFrame{};
			argFrame.srcPos = expression.srcPos;
			argFrame.type = Instruction::Type::PushArgFrame;
			instructions.push_back(std::move(argFrame));

			CompileExpression(expression.children[0], instructions);

			Instruction dot{};
			dot.srcPos = expression.srcPos;
			dot.type = Instruction::Type::Dot;
			dot.string = std::make_unique<StringArgInstruction>();
			dot.string->string = "__getitem__";
			instructions.push_back(std::move(dot));

			for (size_t i = 1; i < expression.children.size(); i++)
				CompileExpression(expression.children[i], instructions);
			
			Instruction slice{};
			slice.srcPos = expression.srcPos;
			slice.type = Instruction::Type::Slice;
			instructions.push_back(std::move(slice));

			instr.type = Instruction::Type::Call;
			break;
		}
		case Operation::ListComprehension: {
			Instruction argFrame{};
			argFrame.srcPos = expression.srcPos;
			argFrame.type = Instruction::Type::PushArgFrame;
			instructions.push_back(std::move(argFrame));
			
			Instruction list{};
			list.srcPos = expression.srcPos;
			list.type = Instruction::Type::List;
			instructions.push_back(std::move(list));
			
			Instruction assign{};
			assign.srcPos = expression.srcPos;
			assign.type = Instruction::Type::DirectAssign;
			assign.directAssign = std::make_unique<DirectAssignInstruction>();
			assign.directAssign->assignTarget.type = AssignType::Direct;
			assign.directAssign->assignTarget.direct = expression.listComp.listName;
			instructions.push_back(std::move(assign));
			
			CompileBody(expression.listComp.forBody, instructions);
			return;
		}
		case Operation::Function:
			CompileFunction(expression, instructions);
			return;
		case Operation::Kwarg: {
			Instruction load{};
			load.srcPos = expression.srcPos;
			load.type = Instruction::Type::Literal;
			load.literal = std::make_unique<LiteralInstruction>();
			*load.literal = expression.variableName;
			instructions.push_back(std::move(load));

			Instruction push{};
			push.srcPos = expression.srcPos;
			push.type = Instruction::Type::PushKwarg;
			instructions.push_back(std::move(push));

			compileChildExpressions();
			return;
		}
		case Operation::CompoundAssignment:
			CompileAssignment(expression.assignTarget, expression.children[0].children[0], expression.children[0], expression.srcPos, instructions);
			return;
		default: {
			Instruction argFrame{};
			argFrame.srcPos = expression.srcPos;
			argFrame.type = Instruction::Type::PushArgFrame;
			instructions.push_back(std::move(argFrame));

			CompileExpression(expression.children[0], instructions);

			Instruction dot{};
			dot.srcPos = expression.srcPos;
			dot.type = Instruction::Type::Dot;
			dot.string = std::make_unique<StringArgInstruction>();
			dot.string->string = OP_METHODS.at(expression.operation);
			instructions.push_back(std::move(dot));

			for (size_t i = 1; i < expression.children.size(); i++)
				CompileExpression(expression.children[i], instructions);

			instr.type = Instruction::Type::Call;
		}
		}

		instructions.push_back(std::move(instr));
	}

	static void CompileExpressionStatement(const Statement& node, std::vector<Instruction>& instructions) {
		CompileExpression(node.expr, instructions);

		Instruction instr{};
		instr.srcPos = node.expr.srcPos;
		instr.type = Instruction::Type::Pop;
		instructions.push_back(std::move(instr));
	}

	static void CompileIf(const Statement& node, std::vector<Instruction>& instructions) {
		CompileExpression(node.expr, instructions);

		size_t falseJumpInstrIndex = instructions.size();
		Instruction falseJump{};
		falseJump.srcPos = node.srcPos;
		falseJump.type = Instruction::Type::JumpIfFalsePop;
		falseJump.jump = std::make_unique<JumpInstruction>();
		instructions.push_back(std::move(falseJump));

		CompileBody(node.body, instructions);

		if (node.elseClause) {
			size_t trueJumpInstrIndex = instructions.size();
			Instruction trueJump{};
			trueJump.srcPos = node.elseClause->srcPos;
			trueJump.type = Instruction::Type::Jump;
			trueJump.jump = std::make_unique<JumpInstruction>();
			instructions.push_back(std::move(trueJump));

			instructions[falseJumpInstrIndex].jump->location = instructions.size();

			CompileBody(node.elseClause->body, instructions);

			instructions[trueJumpInstrIndex].jump->location = instructions.size();
		} else {
			instructions[falseJumpInstrIndex].jump->location = instructions.size();
		}
	}

	static void CompileWhile(const Statement& node, std::vector<Instruction>& instructions) {
		size_t conditionLocation = instructions.size();
		CompileExpression(node.expr, instructions);
		
		size_t terminateJumpInstrIndex = instructions.size();
		Instruction terminateJump{};
		terminateJump.srcPos = node.srcPos;
		terminateJump.type = Instruction::Type::JumpIfFalsePop;
		terminateJump.jump = std::make_unique<JumpInstruction>();
		instructions.push_back(std::move(terminateJump));

		CompileBody(node.body, instructions);

		Instruction loopJump{};
		loopJump.srcPos = node.srcPos;
		loopJump.type = Instruction::Type::Jump;
		loopJump.jump = std::make_unique<JumpInstruction>();
		loopJump.jump->location = conditionLocation;
		instructions.push_back(std::move(loopJump));

		instructions[terminateJumpInstrIndex].jump->location = instructions.size();

		if (node.elseClause) {
			CompileBody(node.elseClause->body, instructions);
		}

		for (size_t index : breakInstructions) {
			instructions[index].jump->location = instructions.size();
		}
		for (size_t index : continueInstructions) {
			instructions[index].jump->location = conditionLocation;
		}
		breakInstructions.clear();
		continueInstructions.clear();
	}

	static void CompileBreak(const Statement& node, std::vector<Instruction>& instructions) {
		breakInstructions.push_back(instructions.size());

		Instruction jump{};
		jump.srcPos = node.srcPos;
		jump.type = Instruction::Type::Jump;
		jump.jump = std::make_unique<JumpInstruction>();
		instructions.push_back(std::move(jump));
	}

	static void CompileContinue(const Statement& node, std::vector<Instruction>& instructions) {
		continueInstructions.push_back(instructions.size());

		Instruction jump{};
		jump.srcPos = node.srcPos;
		jump.type = Instruction::Type::Jump;
		jump.jump = std::make_unique<JumpInstruction>();
		instructions.push_back(std::move(jump));
	}

	static void CompileReturn(const Statement& node, std::vector<Instruction>& instructions) {
		CompileExpression(node.expr, instructions);

		Instruction in{};
		in.srcPos = node.srcPos;
		in.type = Instruction::Type::Return;
		instructions.push_back(std::move(in));
	}

	static void CompileFunction(const Expression& node, std::vector<Instruction>& instructions) {
		const auto& parameters = node.def.parameters;
		size_t defaultParamCount = 0;
		for (size_t i = parameters.size(); i-- > 0; ) {
			const auto& param = parameters[i];
			if (param.defaultValue.has_value()) {
				CompileExpression(param.defaultValue.value(), instructions);
				defaultParamCount = parameters.size() - i;
			} else {
				break;
			}
		}

		Instruction def{};
		def.srcPos = node.srcPos;
		def.type = Instruction::Type::Def;
		def.def = std::make_unique<DefInstruction>();
		def.def->variables = std::vector<std::string>(
			node.def.variables.begin(),
			node.def.variables.end()
			);
		def.def->localCaptures = std::vector<std::string>(
			node.def.localCaptures.begin(),
			node.def.localCaptures.end()
			);
		def.def->globalCaptures = std::vector<std::string>(
			node.def.globalCaptures.begin(),
			node.def.globalCaptures.end()
			);
		def.def->instructions = MakeRcPtr<std::vector<Instruction>>();
		def.def->prettyName = node.def.name;
		def.def->defaultParameterCount = defaultParamCount;
		auto& params = def.def->parameters;
		params = std::move(node.def.parameters);
		if (!params.empty() && params.back().type == Parameter::Type::Kwargs) {
			def.def->kwArgs = std::move(params.back().name);
			params.pop_back();
		}
		if (!params.empty() && params.back().type == Parameter::Type::ListArgs) {
			def.def->listArgs = std::move(params.back().name);
			params.pop_back();
		}
		CompileBody(node.def.body, *def.def->instructions);
		instructions.push_back(std::move(def));
	}

	static void CompileDef(const Statement& node, std::vector<Instruction>& instructions) {
		CompileFunction(node.expr, instructions);

		Instruction assign{};
		assign.srcPos = node.srcPos;
		assign.type = Instruction::Type::DirectAssign;
		assign.directAssign = std::make_unique<DirectAssignInstruction>();
		assign.directAssign->assignTarget.type = AssignType::Direct;
		assign.directAssign->assignTarget.direct = node.expr.def.name;
		instructions.push_back(std::move(assign));

		Instruction pop{};
		pop.srcPos = node.srcPos;
		pop.type = Instruction::Type::Pop;
		instructions.push_back(std::move(pop));
	}

	static void CompileClass(const Statement& node, std::vector<Instruction>& instructions) {
		for (const auto& child : node.body) {
			CompileDef(child, instructions);
			instructions.pop_back();
			instructions.pop_back();
			instructions.back().def->isMethod = true;
		}

		Instruction argFrame{};
		argFrame.srcPos = node.srcPos;
		argFrame.type = Instruction::Type::PushArgFrame;
		instructions.push_back(std::move(argFrame));

		for (const auto& base : node.klass.bases) {
			CompileExpression(base, instructions);
		}

		Instruction klass{};
		klass.srcPos = node.srcPos;
		klass.type = Instruction::Type::Class;
		klass.klass = std::make_unique<ClassInstruction>();
		klass.klass->methodNames = node.klass.methodNames;
		klass.klass->prettyName = node.klass.name;
		instructions.push_back(std::move(klass));

		Instruction assign{};
		assign.srcPos = node.srcPos;
		assign.type = Instruction::Type::DirectAssign;
		assign.directAssign = std::make_unique<DirectAssignInstruction>();
		assign.directAssign->assignTarget.type = AssignType::Direct;
		assign.directAssign->assignTarget.direct = node.klass.name;
		instructions.push_back(std::move(assign));

		Instruction pop{};
		pop.srcPos = node.srcPos;
		pop.type = Instruction::Type::Pop;
		instructions.push_back(std::move(pop));
	}

	static void CompileImportFrom(const Statement& node, std::vector<Instruction>& instructions) {
		Instruction instr{};
		instr.srcPos = node.srcPos;
		instr.type = Instruction::Type::ImportFrom;
		instr.importFrom = std::make_unique<ImportFromInstruction>();
		instr.importFrom->module = node.importFrom.module;
		instr.importFrom->names = node.importFrom.names;
		instr.importFrom->alias = node.importFrom.alias;
		instructions.push_back(std::move(instr));
	}

	static void CompileImport(const Statement& node, std::vector<Instruction>& instructions) {
		Instruction instr{};
		instr.srcPos = node.srcPos;
		instr.type = Instruction::Type::Import;
		instr.import = std::make_unique<ImportInstruction>();
		instr.import->module = node.import.module;
		instr.import->alias = node.import.alias;
		instructions.push_back(std::move(instr));
	}

	static void CompileRaise(const Statement& node, std::vector<Instruction>& instructions) {
		CompileExpression(node.expr, instructions);

		Instruction raise{};
		raise.srcPos = node.srcPos;
		raise.type = Instruction::Type::Raise;
		instructions.push_back(std::move(raise));
	}

	static void CompileTry(const Statement& node, std::vector<Instruction>& instructions) {
		/* 
		 * Push try
		 * Try body
		 * Jump to finally
		 * Check exception type
		 * Except body
		 * Jump to finally
		 * Finally body
		 * Pop try
		 */

		std::vector<size_t> jumpToFinallyInstructs;
		auto jumpToFinally = [&] {
			jumpToFinallyInstructs.push_back(instructions.size());
			Instruction tryEnd{};
			tryEnd.srcPos = node.srcPos;
			tryEnd.type = Instruction::Type::Jump;
			tryEnd.jump = std::make_unique<JumpInstruction>();
			instructions.push_back(std::move(tryEnd));
		};


		size_t pushTryIndex = instructions.size();
		Instruction pushTry{};
		pushTry.srcPos = node.srcPos;
		pushTry.type = Instruction::Type::PushTry;
		pushTry.pushTry = std::make_unique<TryFrameInstruction>();
		instructions.push_back(std::move(pushTry));

		CompileBody(node.body, instructions);

		jumpToFinally();

		instructions[pushTryIndex].pushTry->exceptJump = instructions.size();
		for (const auto& exceptClause : node.tryBlock.exceptClauses) {
			std::optional<size_t> jumpToNextExceptIndex;
			if (exceptClause.exceptBlock.exceptType.has_value()) {
				Instruction argFrame{};
				argFrame.srcPos = exceptClause.srcPos;
				argFrame.type = Instruction::Type::PushArgFrame;
				instructions.push_back(std::move(argFrame));

				Instruction isInst{};
				isInst.srcPos = exceptClause.srcPos;
				isInst.type = Instruction::Type::IsInstance;
				instructions.push_back(std::move(isInst));

				Instruction curExcept{};
				curExcept.srcPos = exceptClause.srcPos;
				curExcept.type = Instruction::Type::CurrentException;
				instructions.push_back(std::move(curExcept));

				CompileExpression(exceptClause.exceptBlock.exceptType.value(), instructions);

				Instruction call{};
				call.srcPos = exceptClause.srcPos;
				call.type = Instruction::Type::Call;
				instructions.push_back(std::move(call));

				jumpToNextExceptIndex = instructions.size();
				Instruction jumpToNextExcept{};
				jumpToNextExcept.srcPos = exceptClause.srcPos;
				jumpToNextExcept.type = Instruction::Type::JumpIfFalsePop;
				jumpToNextExcept.jump = std::make_unique<JumpInstruction>();
				instructions.push_back(std::move(jumpToNextExcept));

				if (!exceptClause.exceptBlock.var.empty()) {
					Instruction curExcept{};
					curExcept.srcPos = exceptClause.srcPos;
					curExcept.type = Instruction::Type::CurrentException;
					instructions.push_back(std::move(curExcept));

					Instruction assign{};
					assign.srcPos = exceptClause.srcPos;
					assign.type = Instruction::Type::DirectAssign;
					assign.directAssign = std::make_unique<DirectAssignInstruction>();
					assign.directAssign->assignTarget.type = AssignType::Direct;
					assign.directAssign->assignTarget.direct = exceptClause.exceptBlock.var;
					instructions.push_back(std::move(assign));

					Instruction pop{};
					pop.srcPos = exceptClause.srcPos;
					pop.type = Instruction::Type::Pop;
					instructions.push_back(std::move(pop));
				}
			}

			Instruction except{};
			except.srcPos = exceptClause.srcPos;
			except.type = Instruction::Type::Except;
			instructions.push_back(std::move(except));

			CompileBody(exceptClause.body, instructions);

			jumpToFinally();

			if (jumpToNextExceptIndex.has_value()) {
				instructions[jumpToNextExceptIndex.value()].jump->location = instructions.size();
			}
		}
		
		instructions[pushTryIndex].pushTry->finallyJump = instructions.size();
		for (size_t instrIndex : jumpToFinallyInstructs) {
			instructions[instrIndex].jump->location = instructions.size();
		}

		CompileBody(node.tryBlock.finallyClause, instructions);
		
		Instruction popTry{};
		popTry.srcPos = node.srcPos;
		popTry.type = Instruction::Type::PopTry;
		popTry.jump = std::make_unique<JumpInstruction>();
		instructions.push_back(std::move(popTry));
	}

	using CompileFn = void(*)(const Statement&, std::vector<Instruction>&);

	static const std::unordered_map<Statement::Type, CompileFn> COMPILE_FUNCTIONS = {
		{ Statement::Type::Expr, CompileExpressionStatement },
		{ Statement::Type::If, CompileIf },
		{ Statement::Type::While, CompileWhile },
		{ Statement::Type::Break, CompileBreak },
		{ Statement::Type::Continue, CompileContinue },
		{ Statement::Type::Return, CompileReturn },
		{ Statement::Type::Def, CompileDef },
		{ Statement::Type::Class, CompileClass },
		{ Statement::Type::Try, CompileTry },
		{ Statement::Type::Raise, CompileRaise },
		{ Statement::Type::Import, CompileImport },
		{ Statement::Type::ImportFrom, CompileImportFrom },
		{ Statement::Type::Pass, [](auto, auto) {}},
		{ Statement::Type::Global, [](auto, auto) {}},
		{ Statement::Type::Nonlocal, [](auto, auto) {}},
	};

	static void CompileStatement(const Statement& node, std::vector<Instruction>& instructions) {
		COMPILE_FUNCTIONS.at(node.type)(node, instructions);
	}

	static void CompileBody(const std::vector<Statement>& body, std::vector<Instruction>& instructions) {
		for (const auto& child : body) {
			CompileStatement(child, instructions);
		}
	}

	std::vector<Instruction> Compile(const Statement& parseTree) {
		breakInstructions.clear();
		continueInstructions.clear();

		std::vector<Instruction> instructions;
		CompileBody(parseTree.expr.def.body, instructions);

		return instructions;
	}

}


namespace wings {
	bool ImportDis(Wg_Context* context);
}


#include <queue>

namespace wings {
	namespace dismodule {
		static std::string AssignTargetToString(const AssignTarget& target) {
			if (target.type == AssignType::Direct) {
				return target.direct;
			} else {
				std::string s = "(";
				for (const auto& child : target.pack) {
					s += AssignTargetToString(child);
					s += ", ";
				}
				s.pop_back();
				s.pop_back();
				return s;
			}
		}

		static std::string LiteralToString(const LiteralInstruction& literal) {
			if (std::holds_alternative<std::nullptr_t>(literal)) {
				return "None";
			} else if (std::holds_alternative<bool>(literal)) {
				return std::get<bool>(literal) ? "True" : "False";
			} else if (std::holds_alternative<Wg_int>(literal)) {
				return std::to_string(std::get<Wg_int>(literal));
			} else if (std::holds_alternative<Wg_float>(literal)) {
				return std::to_string(std::get<Wg_float>(literal));
			} else if (std::holds_alternative<std::string>(literal)) {
				return "\"" + std::get<std::string>(literal) + "\"";
			} else {
				WG_UNREACHABLE();
			}
		}

		static std::string PadLeft(size_t i, size_t size) {
			auto n = std::to_string(i);
			while (n.size() < size)
				n.insert(0, 1, ' ');
			return n;
		}

		static Wg_Obj* dis(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_FUNC(0);

			const auto& fn = argv[0]->Get<Wg_Obj::Func>();
			if (fn.fptr != &DefObject::Run) {
				Wg_RaiseException(context, WG_EXC_TYPEERROR, "Cannot disassemble native function");
				return nullptr;
			}

			struct Func {
				const std::vector<Instruction>* instructions;
				std::string_view name;
			};

			std::queue<Func> functions;
			DefObject* def = (DefObject*)fn.userdata;
			functions.push(Func{ &*def->instructions, def->prettyName });

			std::string s;
			while (!functions.empty()) {
				s += "Function ";
				s += functions.front().name;
				s += "()\n";
				const auto* instructions = functions.front().instructions;
				functions.pop();

				for (size_t i = 0; i < instructions->size(); i++) {
					const Instruction& instr = (*instructions)[i];

					if (i == 0 || instr.srcPos.line != (*instructions)[i - 1].srcPos.line) {
						if (i)
							s += "\n";
						s += PadLeft(instr.srcPos.line + 1, 6) + " ";
					} else {
						s += "       ";
					}
					s += PadLeft(i, 4) + " ";

					switch (instr.type) {
					case Instruction::Type::DirectAssign:
						if (instr.directAssign->assignTarget.type == AssignType::Direct) {
							s += "ASSIGN\t\t";
						} else {
							s += "ASSIGN_PACK\t\t";
						}
						s += AssignTargetToString(instr.directAssign->assignTarget);
						break;
					case Instruction::Type::MemberAssign:
						s += "ASSIGN_ATTR\t\t" + instr.string->string;
						break;
					case Instruction::Type::Literal:
						s += "LOAD_CONST\t\t" + LiteralToString(*instr.literal);
						break;
					case Instruction::Type::Call:
						s += "CALL";
						break;
					case Instruction::Type::Return:
						s += "RETURN";
						break;
					case Instruction::Type::Pop:
						s += "POP";
						break;
					case Instruction::Type::PushArgFrame:
						s += "BEGIN_ARGS";
						break;
					case Instruction::Type::Dot:
						s += "GET_ATTR\t\t" + instr.string->string;
						break;
					case Instruction::Type::Variable:
						s += "LOAD_VAR\t\t" + instr.string->string;
						break;
					case Instruction::Type::Jump:
						s += "JUMP\t\tto " + std::to_string(instr.jump->location);
						break;
					case Instruction::Type::JumpIfFalsePop:
						s += "JUMP_IF_FALSE_POP\tto " + std::to_string(instr.jump->location);
						break;
					case Instruction::Type::JumpIfFalse:
						s += "JUMP_IF_FALSE\tto " + std::to_string(instr.jump->location);
						break;
					case Instruction::Type::JumpIfTrue:
						s += "JUMP_IF_TRUE\tto " + std::to_string(instr.jump->location);
						break;
					case Instruction::Type::List:
						s += "MAKE_LIST";
						break;
					case Instruction::Type::Tuple:
						s += "MAKE_TUPLE";
						break;
					case Instruction::Type::Map:
						s += "MAKE_DICT";
						break;
					case Instruction::Type::Set:
						s += "MAKE_SET";
						break;
					case Instruction::Type::Slice:
						s += "MAKE_SLICE";
						break;
					case Instruction::Type::Raise:
						s += "RAISE";
						break;
					case Instruction::Type::PushTry:
						s += "BEGIN_TRY\t\t" + std::to_string(instr.pushTry->exceptJump)
							+ ", " + std::to_string(instr.pushTry->finallyJump);
						break;
					case Instruction::Type::PopTry:
						s += "END_TRY";
						break;
					case Instruction::Type::CurrentException:
						s += "LOAD_CUR_EXCEPT";
						break;
					case Instruction::Type::IsInstance:
						s += "LOAD_IS_INSTANCE";
						break;
					case Instruction::Type::Except:
						s += "HANDLE_EXCEPT";
						break;
					case Instruction::Type::Import:
						s += "IMPORT\t\t" + instr.import->module;
						if (!instr.import->alias.empty())
							s += " as " + instr.import->alias;
						break;
					case Instruction::Type::ImportFrom:
						if (instr.importFrom->names.empty()) {
							s += "IMPORT_ALL\t\t" + instr.importFrom->module;
						} else if (!instr.importFrom->alias.empty()) {
							s += "IMPORT_FROM\t\tfrom " + instr.importFrom->module
								+ " import " + instr.importFrom->names[0]
								+ " as " + instr.importFrom->alias;
						} else {
							s += "IMPORT_FROM\t\tfrom " + instr.importFrom->module + " import ";
							for (const auto& name : instr.importFrom->names) {
								s += name + ", ";
							}
							s.pop_back();
							s.pop_back();
						}
						break;
					case Instruction::Type::Is:
						s += "IS";
						break;
					case Instruction::Type::PushKwarg:
						s += "PUSH_KWARG";
						break;
					case Instruction::Type::UnpackMapForCall:
						s += "UNPACK_KWARGS";
						break;
					case Instruction::Type::UnpackMapForMapCreation:
						s += "UNPACK_DICT";
						break;
					case Instruction::Type::Unpack:
						s += "UNPACK_ITERABLE";
						break;
					case Instruction::Type::Class:
						s += "MAKE_CLASS\t\t" + instr.klass->prettyName + " [";
						for (const auto& name : instr.klass->methodNames) {
							s += name + ", ";
						}
						s.pop_back();
						s.pop_back();
						s += "]";
						break;
					case Instruction::Type::Def:
						s += "MAKE_FUNCTION\t" + instr.def->prettyName;

						functions.push(Func{ &*instr.def->instructions, instr.def->prettyName });
						break;
					default:
						s += "???";
						break;
					}

					s += "\n";
				}

				s += "\n";
			}

			Wg_Print(context, s.c_str(), (int)s.size());

			return Wg_None(context);
		}
	}

	bool ImportDis(Wg_Context* context) {
		using namespace dismodule;
		try {
			RegisterFunction(context, "dis", dis);
			return true;
		} catch (LibraryInitException&) {
			return false;
		}
	}
}


namespace wings {

	Wg_Obj* DefObject::Run(Wg_Context* context, Wg_Obj** args, int argc) {
		DefObject* def = (DefObject*)Wg_GetFunctionUserdata(context);
		Wg_Obj* kwargs = Wg_GetKwargs(context);
		if (kwargs == nullptr)
			return nullptr;

		Executor executor{};
		executor.def = def;
		executor.context = context;

		// Create local variables
		for (const auto& localVar : def->localVariables) {
			Wg_Obj* null = Wg_None(def->context);
			executor.variables.insert({ localVar, MakeRcPtr<Wg_Obj*>(null) });
		}

		// Add captures
		for (const auto& capture : def->captures) {
			executor.variables.insert(capture);
		}

		// Initialise parameters

		// Set kwargs
		Wg_Obj* newKwargs = nullptr;
		Wg_ObjRef ref;
		if (def->kwArgs.has_value()) {
			newKwargs = Wg_NewDictionary(context);
			if (newKwargs == nullptr)
				return nullptr;
			ref = Wg_ObjRef(newKwargs);
			executor.variables.insert({ def->kwArgs.value(), MakeRcPtr<Wg_Obj*>(newKwargs)});
		}

		std::vector<bool> assignedParams(def->parameterNames.size());
		if (kwargs) {
			for (const auto& [k, value] : kwargs->Get<WDict>()) {
				const char* key = Wg_GetString(k);
				bool found = false;
				for (size_t i = 0; i < def->parameterNames.size(); i++) {
					if (def->parameterNames[i] == key) {
						executor.variables.insert({ key, MakeRcPtr<Wg_Obj*>(value) });
						assignedParams[i] = true;
						found = true;
						break;
					}
				}

				if (!found) {
					if (newKwargs == nullptr) {
						std::string msg;
						if (!def->prettyName.empty())
							msg = def->prettyName + "() ";
						msg += std::string("got an unexpected keyword argument '") + key + "'";
						Wg_RaiseException(context, WG_EXC_TYPEERROR, msg.c_str());
						return nullptr;
					}

					try {
						newKwargs->Get<WDict>()[k] = value;
					} catch (HashException&) {
						return nullptr;
					}
				}
			}
		}

		// Set positional args
		Wg_Obj* listArgs = nullptr;
		if (def->listArgs.has_value()) {
			listArgs = Wg_NewTuple(context, nullptr, 0);
			if (listArgs == nullptr)
				return nullptr;
			executor.variables.insert({ def->listArgs.value(), MakeRcPtr<Wg_Obj*>(listArgs) });
		}

		for (size_t i = 0; i < (size_t)argc; i++) {
			if (i < def->parameterNames.size()) {
				if (assignedParams[i]) {
					std::string msg;
					if (!def->prettyName.empty())
						msg = def->prettyName + "() ";
					msg += "got multiple values for argument '" + def->parameterNames[i] + "'";
					Wg_RaiseException(context, WG_EXC_TYPEERROR, msg.c_str());
					return nullptr;
				}
				executor.variables.insert({ def->parameterNames[i], MakeRcPtr<Wg_Obj*>(args[i]) });
				assignedParams[i] = true;
			} else {
				if (listArgs == nullptr) {
					std::string msg;
					if (!def->prettyName.empty())
						msg = def->prettyName + "() ";
					msg += "takes " + std::to_string(def->parameterNames.size())
						+ " positional argument(s) but " + std::to_string(argc)
						+ (argc == 1 ? " was given" : " were given");
					Wg_RaiseException(context, WG_EXC_TYPEERROR, msg.c_str());
					return nullptr;
				}
				listArgs->Get<std::vector<Wg_Obj*>>().push_back(args[i]);
			}
		}
		
		// Set default args
		size_t defaultableArgsStart = def->parameterNames.size() - def->defaultParameterValues.size();
		for (size_t i = 0; i < def->defaultParameterValues.size(); i++) {
			size_t index = defaultableArgsStart + i;
			if (!assignedParams[index]) {
				executor.variables.insert({ def->parameterNames[index], MakeRcPtr<Wg_Obj*>(def->defaultParameterValues[i]) });
				assignedParams[index] = true;
			}
		}

		// Check for unassigned arguments
		std::string unassigned;
		for (size_t i = 0; i < def->parameterNames.size(); i++)
			if (!assignedParams[i])
				unassigned += std::to_string(i + 1) + ", ";
		if (!unassigned.empty()) {
			unassigned.pop_back();
			unassigned.pop_back();
			std::string msg = "Function " +
				def->prettyName + "()"
				+ " missing parameter(s) "
				+ unassigned;
			Wg_RaiseException(context, WG_EXC_TYPEERROR, msg.c_str());
			return nullptr;
		}

		context->executors.push_back(&executor);
		auto result = executor.Run();
		context->executors.pop_back();
		return result;
	}

	void  Executor::PushStack(Wg_Obj* obj) {
		stack.push_back(obj);
	}

	Wg_Obj* Executor::PopStack() {
		auto obj = stack.back();
		stack.pop_back();
		return obj;
	}

	void Executor::PopStackUntil(size_t size) {
		while (stack.size() > size)
			PopStack();
	}

	Wg_Obj* Executor::PeekStack() {
		return stack.back();
	}

	void Executor::ClearStack() {
		while (!stack.empty())
			PopStack();
		argFrames = {};
		kwargsStack = {};
	}

	size_t Executor::PopArgFrame() {
		kwargsStack.pop_back();
		size_t ret = stack.size() - argFrames.top();
		argFrames.pop();
		return ret;
	}

	Wg_Obj* Executor::GetVariable(const std::string& name) {
		auto it = variables.find(name);
		if (it != variables.end()) {
			return *it->second;
		} else {
			return Wg_GetGlobal(context, name.c_str());
		}
	}

	void Executor::SetVariable(const std::string& name, Wg_Obj* value) {
		auto it = variables.find(name);
		if (it != variables.end()) {
			*it->second = value;
		} else {
			Wg_SetGlobal(context, name.c_str(), value);
		}
	}

	Wg_Obj* Executor::DirectAssign(const AssignTarget& target, Wg_Obj* value) {
		switch (target.type) {
		case AssignType::Direct:
			SetVariable(target.direct, value);
			return value;
		case AssignType::Pack: {
			std::vector<Wg_ObjRef> values;
			auto f = [](Wg_Obj* value, void* userdata) {
				((std::vector<Wg_ObjRef>*)userdata)->emplace_back(value);
				return true;
			};

			if (!Wg_Iterate(value, &values, f))
				return nullptr;

			if (values.size() != target.pack.size()) {
				Wg_RaiseException(context, WG_EXC_TYPEERROR, "Packed assignment argument count mismatch");
				return nullptr;
			}

			for (size_t i = 0; i < values.size(); i++)
				if (!DirectAssign(target.pack[i], values[i].Get()))
					return nullptr;

			std::vector<Wg_Obj*> buf;
			for (const auto& v : values)
				buf.push_back(v.Get());
			return Wg_NewTuple(context, buf.data(), (int)buf.size());
		}
		default:
			WG_UNREACHABLE();
		}
	}

	Wg_Obj* Executor::Run() {
		auto& frame = context->currentTrace.back();
		frame.module = def->module;
		frame.func = def->prettyName;

		for (pc = 0; pc < def->instructions->size(); pc++) {
			const auto& instr = (*def->instructions)[pc];
			
			auto& frame = context->currentTrace.back();
			frame.lineText = (*def->originalSource)[instr.srcPos.line];
			frame.srcPos = instr.srcPos;

			DoInstruction(instr);

			if (!exitValue.has_value())
				continue;

			// Return normally
			if (exitValue.value() != nullptr)
				break;

			// New exception thrown
			if (tryFrames.empty()) {
				// No handlers. Propagate to next function.
				break;
			}
			
			PopStackUntil(tryFrames.top().stackSize);
			if (tryFrames.top().isHandlingException) {
				// Was already handling an exception. Jump to finally.
				pc = tryFrames.top().finallyJump - 1;
				exitValue.reset();
			} else {
				// Jump to handler
				pc = tryFrames.top().exceptJump - 1;
				tryFrames.top().isHandlingException = true;
				exitValue.reset();
			}
		}

		ClearStack();

		if (exitValue.has_value()) {
			return exitValue.value();
		} else {
			return Wg_None(context);
		}
	}

	void Executor::DoInstruction(const Instruction& instr) {
		switch (instr.type) {
		case Instruction::Type::Jump:
			pc = instr.jump->location - 1;
			break;
		case Instruction::Type::JumpIfFalsePop:
			if (Wg_Obj* truthy = Wg_UnaryOp(WG_UOP_BOOL, PopStack())) {
				if (!Wg_GetBool(truthy)) {
					pc = instr.jump->location - 1;
				}
			} else {
				exitValue = nullptr;
			}
			break;
		case Instruction::Type::JumpIfFalse:
		case Instruction::Type::JumpIfTrue:
			if (Wg_Obj* truthy = Wg_UnaryOp(WG_UOP_BOOL, PeekStack())) {
				if (Wg_GetBool(truthy) == (instr.type == Instruction::Type::JumpIfTrue)) {
					pc = instr.jump->location - 1;
				}
			} else {
				exitValue = nullptr;
			}
			break;
		case Instruction::Type::Pop:
			PopStack();
			break;
		case Instruction::Type::Return:
			exitValue = PopStack();
			break;
		case Instruction::Type::Def: {
			DefObject* def = new DefObject();
			def->context = context;
			def->module = this->def->module;
			def->prettyName = instr.def->prettyName;
			def->instructions = instr.def->instructions;
			def->originalSource = this->def->originalSource;

			for (const auto& param : instr.def->parameters)
				def->parameterNames.push_back(param.name);
			for (size_t i = 0; i < instr.def->defaultParameterCount; i++)
				def->defaultParameterValues.push_back(PopStack());
			def->listArgs = instr.def->listArgs;
			def->kwArgs = instr.def->kwArgs;

			const auto& module = std::string(context->currentModule.top());
			auto& globals = context->globals.at(module);
			
			for (const auto& capture : instr.def->localCaptures) {
				if (variables.contains(capture)) {
					def->captures.insert({ capture, variables[capture] });
				} else {
					if (!globals.contains(capture))
						Wg_SetGlobal(context, capture.c_str(), Wg_None(context));
					
					def->captures.insert({ capture, globals.at(capture) });
				}
			}
			for (const auto& capture : instr.def->globalCaptures) {
				def->captures.insert({ capture, globals.at(capture) });
			}
			def->localVariables = instr.def->variables;

			Wg_Obj* obj = Wg_NewFunction(context, &DefObject::Run, def, instr.def->prettyName.c_str());
			if (obj == nullptr) {
				delete def;
				exitValue = nullptr;
				return;
			}
			obj->Get<Wg_Obj::Func>().isMethod = instr.def->isMethod;

			Wg_RegisterFinalizer(obj, [](void* userdata) { delete (DefObject*)userdata; }, def);

			PushStack(obj);
			break;
		}
		case Instruction::Type::Class: {
			size_t methodCount = instr.klass->methodNames.size();
			size_t baseCount = PopArgFrame();
			auto stackEnd = stack.data() + stack.size();

			std::vector<const char*> methodNames;
			for (const auto& methodName : instr.klass->methodNames)
				methodNames.push_back(methodName.c_str());

			Wg_Obj** bases = stackEnd - baseCount;
			Wg_Obj** methods = stackEnd - methodCount - baseCount;

			Wg_Obj* klass = Wg_NewClass(context, instr.klass->prettyName.c_str(), bases, (int)baseCount);
			if (klass == nullptr) {
				exitValue = nullptr;
				return;
			}

			for (size_t i = 0; i < methodCount; i++)
				AddAttributeToClass(klass, instr.klass->methodNames[i].c_str(), methods[i]);

			for (size_t i = 0; i < methodCount + baseCount; i++)
				PopStack();

			if (klass == nullptr) {
				exitValue = nullptr;
			} else {
				PushStack(klass);
			}
			break;
		}
		case Instruction::Type::Literal: {
			Wg_Obj* value{};
			if (std::holds_alternative<std::nullptr_t>(*instr.literal)) {
				value = Wg_None(context);
			} else if (auto* b = std::get_if<bool>(instr.literal.get())) {
				value = Wg_NewBool(context, *b);
			} else if (auto* i = std::get_if<Wg_int>(instr.literal.get())) {
				value = Wg_NewInt(context, *i);
			} else if (auto* f = std::get_if<Wg_float>(instr.literal.get())) {
				value = Wg_NewFloat(context, *f);
			} else if (auto* s = std::get_if<std::string>(instr.literal.get())) {
				value = Wg_NewStringBuffer(context, s->c_str(), (int)s->size());
			} else {
				WG_UNREACHABLE();
			}

			if (value) {
				PushStack(value);
			} else {
				exitValue = nullptr;
			}
			break;
		}
		case Instruction::Type::Tuple:
		case Instruction::Type::List:
		case Instruction::Type::Set: {
			Wg_Obj* (*creator)(Wg_Context*, Wg_Obj**, int) = nullptr;
			switch (instr.type) {
			case Instruction::Type::Tuple: creator = Wg_NewTuple; break;
			case Instruction::Type::List: creator = Wg_NewList; break;
			case Instruction::Type::Set: creator = Wg_NewSet; break;
			}
			size_t argc = PopArgFrame();
			Wg_Obj** argv = stack.data() + stack.size() - argc;
			if (Wg_Obj* li = creator(context, argv, (int)argc)) {
				for (size_t i = 0; i < argc; i++)
					PopStack();
				PushStack(li);
			} else {
				exitValue = nullptr;
			}
			break;
		}
		case Instruction::Type::Map:
			if (Wg_Obj* dict = Wg_NewDictionary(context)) {
				size_t argc = PopArgFrame();
				Wg_Obj** start = stack.data() + stack.size() - argc;
				for (size_t i = 0; i < argc / 2; i++) {
					Wg_Obj* key = start[2 * i];
					Wg_Obj* val = start[2 * i + 1];
					Wg_ObjRef ref(dict);
					try {
						dict->Get<WDict>()[key] = val;
					} catch (HashException&) {
						exitValue = nullptr;
						return;
					}
				}

				for (size_t i = 0; i < argc; i++)
					PopStack();
				PushStack(dict);
			} else {
				exitValue = nullptr;
			}
			break;
		case Instruction::Type::Variable:
			if (Wg_Obj* value = GetVariable(instr.string->string)) {
				PushStack(value);
			} else {
				Wg_RaiseNameError(context, instr.string->string.c_str());
				exitValue = nullptr;
			}
			break;
		case Instruction::Type::DirectAssign: {
			if (Wg_Obj* v = DirectAssign(instr.directAssign->assignTarget, PopStack())) {
				PushStack(v);
			} else {
				exitValue = nullptr;
			}
			break;
		}
		case Instruction::Type::MemberAssign: {
			Wg_Obj* value = PopStack();
			Wg_Obj* obj = PopStack();
			Wg_SetAttribute(obj, instr.string->string.c_str(), value);
			PushStack(value);
			break;
		}
		case Instruction::Type::PushArgFrame:
			argFrames.push(stack.size());
			kwargsStack.push_back({});
			break;
		case Instruction::Type::Call: {
			size_t kwargc = kwargsStack.back().size();
			size_t argc = stack.size() - argFrames.top() - kwargc - 1;

			Wg_Obj* fn = stack[stack.size() - argc - kwargc - 1];
			Wg_Obj** args = stack.data() + stack.size() - argc - kwargc;
			Wg_Obj** kwargsv = stack.data() + stack.size() - kwargc;

			Wg_Obj* kwargs = nullptr;
			if (kwargc) {
				kwargs = Wg_NewDictionary(context, kwargsStack.back().data(), kwargsv, (int)kwargc);
				if (kwargs == nullptr) {
					exitValue = nullptr;
					return;
				}
			}

			if (Wg_Obj* ret = Wg_Call(fn, args, (int)argc, kwargs)) {
				for (size_t i = 0; i < argc + kwargc + 1; i++)
					PopStack();
				PushStack(ret);
			} else {
				exitValue = nullptr;
			}
			PopArgFrame();
			break;
		}
		case Instruction::Type::Dot: {
			Wg_Obj* obj = PopStack();
			if (Wg_Obj* attr = Wg_GetAttribute(obj, instr.string->string.c_str())) {
				PushStack(attr);
			} else {
				exitValue = nullptr;
			}
			break;
		}
		case Instruction::Type::Unpack: {
			Wg_Obj* iterable = PopStack();

			auto f = [](Wg_Obj* value, void* userdata) {
				Executor* executor = (Executor*)userdata;
				executor->PushStack(value);
				return true;
			};

			if (!Wg_Iterate(iterable, this, f)) {
				exitValue = nullptr;
			}
			break;
		}
		case Instruction::Type::UnpackMapForMapCreation: {
			Wg_Obj* map = PopStack();
			if (!Wg_IsDictionary(map)) {
				Wg_RaiseException(context, WG_EXC_TYPEERROR, "Unary '**' must be applied to a dictionary");
				exitValue = nullptr;
				return;
			}

			for (const auto& [key, value] : map->Get<WDict>()) {
				PushStack(key);
				PushStack(value);
			}
			break;
		}
		case Instruction::Type::UnpackMapForCall: {
			Wg_Obj* map = PopStack();
			if (!Wg_IsDictionary(map)) {
				Wg_RaiseException(context, WG_EXC_TYPEERROR, "Unary '**' must be applied to a dictionary");
				exitValue = nullptr;
				return;
			}

			for (const auto& [key, value] : map->Get<WDict>()) {
				if (!Wg_IsString(key)) {
					Wg_RaiseException(context, WG_EXC_TYPEERROR, "Keywords must be strings");
					exitValue = nullptr;
					return;
				}
				kwargsStack.back().push_back(key);
				PushStack(value);
			}
			break;
		}
		case Instruction::Type::PushKwarg:
			kwargsStack.back().push_back(PopStack());
			break;
		case Instruction::Type::Not: {
			Wg_Obj* arg = Wg_UnaryOp(WG_UOP_BOOL, PopStack());
			if (arg == nullptr) {
				exitValue = nullptr;
				break;
			}

			if (Wg_Obj* value = Wg_NewBool(context, !Wg_GetBool(arg))) {
				PushStack(value);
			} else {
				exitValue = nullptr;
			}
			break;
		}
		case Instruction::Type::Is:
			PushStack(Wg_NewBool(context, PopStack() == PopStack()));
			break;
		case Instruction::Type::Raise: {
			Wg_Obj* expr = PopStack();
			if (Wg_IsClass(expr)) {
				Wg_RaiseExceptionClass(expr);
			} else {
				Wg_RaiseExceptionObject(expr);
			}
			exitValue = nullptr;
			break;
		}
		case Instruction::Type::PushTry:
			tryFrames.push({
				instr.pushTry->exceptJump,
				instr.pushTry->finallyJump,
				false,
				stack.size()
				});
			break;
		case Instruction::Type::PopTry:
			tryFrames.pop();
			if (Wg_GetException(context))
				exitValue = nullptr;
			break;
		case Instruction::Type::Except:
			Wg_ClearException(context);
			break;
		case Instruction::Type::CurrentException:
			PushStack(Wg_GetException(context));
			break;
		case Instruction::Type::IsInstance:
			PushStack(context->builtins.isinstance);
			break;
		case Instruction::Type::Slice: {
			Wg_Obj* slice = Wg_Call(context->builtins.slice, &context->builtins.none, 1);
			if (slice == nullptr) {
				exitValue = nullptr;
				break;
			}

			Wg_Obj* step = PopStack();
			Wg_Obj* stop = PopStack();
			Wg_Obj* start = PopStack();
			Wg_SetAttribute(slice, "step", step);
			Wg_SetAttribute(slice, "stop", stop);
			Wg_SetAttribute(slice, "start", start);
			PushStack(slice);
			break;
		}
		case Instruction::Type::Import: {
			const char* alias = instr.import->alias.empty() ? nullptr : instr.import->alias.c_str();
			if (Wg_ImportModule(context, instr.import->module.c_str(), alias) == nullptr)
				exitValue = nullptr;
			break;
		}
		case Instruction::Type::ImportFrom: {
			const char* moduleName = instr.importFrom->module.c_str();
			if (instr.importFrom->names.empty()) {
				if (!Wg_ImportAllFromModule(context, moduleName))
					exitValue = nullptr;
			} else if (!instr.importFrom->alias.empty()) {
				if (!Wg_ImportFromModule(context, moduleName, instr.importFrom->names[0].c_str(), instr.importFrom->alias.c_str()))
					exitValue = nullptr;
			} else {
				for (const auto& name : instr.importFrom->names) {
					if (!Wg_ImportFromModule(context, moduleName, name.c_str())) {
						exitValue = nullptr;
						break;
					}
				}
			}
			break;
		}
		default:
			WG_UNREACHABLE();
		}
	}

	void Executor::GetReferences(std::deque<const Wg_Obj*>& refs) {
		for (const auto& var : variables)
			refs.push_back(*var.second);
		for (const auto& frame : kwargsStack)
			for (const auto& kwarg : frame)
				refs.push_back(kwarg);
		for (const auto& val : this->stack)
			refs.push_back(val);
	}
}


#include <unordered_map>

namespace wings {

	static thread_local bool disableInOperator;

	static CodeError ParseExpression(TokenIter& p, Expression& out, size_t minPrecedence, std::optional<Expression> preParsedArg = std::nullopt);

	TokenIter::TokenIter(const std::vector<Token>& tokens) :
		index(0),
		tokens(&tokens)
	{
	}

	TokenIter& TokenIter::operator++() {
		index++;
		return *this;
	}

	TokenIter& TokenIter::operator--() {
		index--;
		return *this;
	}

	const Token& TokenIter::operator*() const {
		return (*tokens)[index];
	}

	const Token* TokenIter::operator->() const {
		return &(*tokens)[index];
	}

	bool TokenIter::operator==(const TokenIter& rhs) const {
		return index == rhs.index && tokens == rhs.tokens;
	}

	bool TokenIter::operator!=(const TokenIter& rhs) const {
		return !(*this == rhs);
	}

	bool TokenIter::EndReached() const {
		return index >= tokens->size();
	}

	static const std::unordered_map<std::string, Operation> BINARY_OP_STRINGS = {
		{ "+",  Operation::Add },
		{ "-",  Operation::Sub },
		{ "*",  Operation::Mul },
		{ "**", Operation::Pow },
		{ "/",  Operation::Div },
		{ "//", Operation::IDiv },
		{ "%",  Operation::Mod },
		{ "<",  Operation::Lt },
		{ ">",  Operation::Gt },
		{ "<=", Operation::Le },
		{ ">=", Operation::Ge },
		{ "==", Operation::Eq },
		{ "!=", Operation::Ne },
		{ "and", Operation::And },
		{ "or", Operation::Or },
		{ "^",  Operation::BitXor },
		{ "&",  Operation::BitAnd },
		{ "|",  Operation::BitOr },
		{ "<<", Operation::ShiftL },
		{ ">>", Operation::ShiftR },
		{ "in", Operation::In },
		{ "not", Operation::NotIn },
		{ "is", Operation::Is },

		{ "=",  Operation::Assign },
		{ ":=",  Operation::Assign },
		{ "+=", Operation::AddAssign },
		{ "-=", Operation::SubAssign },
		{ "*=", Operation::MulAssign },
		{ "**=", Operation::PowAssign },
		{ "/=", Operation::DivAssign },
		{ "//=", Operation::IDivAssign },
		{ "%=", Operation::ModAssign },
		{ "<<=", Operation::ShiftLAssign },
		{ ">>=", Operation::ShiftRAssign },
		{ "|=", Operation::OrAssign },
		{ "&=", Operation::AndAssign },
		{ "^=", Operation::XorAssign },
		{ ".", Operation::Dot },
	};

	static const std::unordered_map<std::string, Operation> PREFIX_UNARY_OP_STRINGS = {
		{ "+", Operation::Pos },
		{ "-", Operation::Neg },
		{ "~", Operation::BitNot },
		{ "not", Operation::Not },
	};

	static const std::unordered_set<Operation> BINARY_OPS = {
		Operation::Add,
		Operation::Sub,
		Operation::Mul,
		Operation::Pow,
		Operation::Div,
		Operation::IDiv,
		Operation::Mod,
		Operation::Lt,
		Operation::Gt,
		Operation::Le,
		Operation::Ge,
		Operation::Eq,
		Operation::Ne,
		Operation::And,
		Operation::Or,
		Operation::BitXor,
		Operation::BitAnd,
		Operation::BitOr,
		Operation::ShiftL,
		Operation::ShiftR,
		Operation::In,
		Operation::NotIn,
		Operation::Is,
		Operation::IsNot,
		Operation::Dot,

		Operation::Assign,
		Operation::AddAssign,
		Operation::SubAssign,
		Operation::MulAssign,
		Operation::PowAssign,
		Operation::DivAssign,
		Operation::IDivAssign,
		Operation::ModAssign,
		Operation::ShiftLAssign,
		Operation::ShiftRAssign,
		Operation::OrAssign,
		Operation::AndAssign,
		Operation::XorAssign,
	};

	static const std::unordered_set<Operation> BINARY_RIGHT_ASSOCIATIVE_OPS = {
		Operation::Assign,
		Operation::AddAssign,
		Operation::SubAssign,
		Operation::MulAssign,
		Operation::PowAssign,
		Operation::DivAssign,
		Operation::IDivAssign,
		Operation::ModAssign,
		Operation::ShiftLAssign,
		Operation::ShiftRAssign,
		Operation::OrAssign,
		Operation::AndAssign,
		Operation::XorAssign,
	};

	static const std::unordered_set<Operation> PREFIX_UNARY_OPS = {
		Operation::Pos,
		Operation::Neg,
		Operation::Not,
		Operation::BitNot,
	};

	static const std::vector<std::vector<Operation>> PRECEDENCE = {
		{ Operation::Call, Operation::Index, Operation::Slice, Operation::Dot },
		{ Operation::Pow },
		{ Operation::Pos, Operation::Neg, Operation::BitNot },
		{ Operation::Mul, Operation::Div, Operation::IDiv, Operation::Mod },
		{ Operation::Add, Operation::Sub },
		{ Operation::ShiftL, Operation::ShiftR },
		{ Operation::BitAnd },
		{ Operation::BitXor },
		{ Operation::BitOr },
		{
			Operation::Eq, Operation::Ne, Operation::Lt, Operation::Le, Operation::Gt,
			Operation::Ge, Operation::In, Operation::NotIn, Operation::Is, Operation::IsNot,
		},
		{ Operation::Not },
		{ Operation::And },
		{ Operation::Or },
		{ Operation::IfElse },
		{
			Operation::Assign, Operation::AddAssign, Operation::SubAssign,
			Operation::MulAssign, Operation::DivAssign, Operation::IDivAssign,
			Operation::ModAssign, Operation::ShiftLAssign, Operation::ShiftRAssign,
			Operation::AndAssign, Operation::OrAssign, Operation::XorAssign, Operation::PowAssign,
		},
	};

	static size_t PrecedenceOf(Operation op) {
		auto it = std::find_if(
			PRECEDENCE.begin(),
			PRECEDENCE.end(),
			[=](const auto& group) { return std::find(group.begin(), group.end(), op) != group.end(); }
		);
		return std::distance(it, PRECEDENCE.end());
	}

	bool IsAssignableExpression(const Expression& expr, AssignTarget& target, bool onlyDirectOrPack) {
		target.type = AssignType::None;
		switch (expr.operation) {
		case Operation::Variable:
			target.type = AssignType::Direct;
			target.direct = expr.variableName;
			return true;
		case Operation::Index:
		case Operation::Slice:
			if (onlyDirectOrPack)
				return false;
			target.type = AssignType::Index;
			return true;
		case Operation::Dot:
			if (onlyDirectOrPack)
				return false;
			target.type = AssignType::Member;
			return true;
		case Operation::Tuple:
		case Operation::List:
			for (const auto& child : expr.children)
				if (!IsAssignableExpression(child, target.pack.emplace_back(), true))
					return false;
			target.type = AssignType::Pack;
			return true;
		default:
			return false;
		}
	}

	CodeError ParseExpressionList(TokenIter& p, const std::string& terminate, std::vector<Expression>& out, bool isFnCall, bool* seenComma) {
		bool mustTerminate = false;
		bool seenKwarg = false;
		if (seenComma) *seenComma = false;
		while (true) {
			// Check for terminating token
			if (p.EndReached()) {
				return CodeError::Bad("Expected a closing bracket", (--p)->srcPos);
			} else if (p->text == terminate) {
				return CodeError::Good();
			} else if (mustTerminate) {
				return CodeError::Bad("Expected a closing bracket", p->srcPos);
			}

			// Check unpack operators
			Operation unpackType{};
			if (p->text == "*") {
				unpackType = Operation::Unpack;
				++p;
			} else if (isFnCall && p->text == "**") {
				unpackType = Operation::UnpackMapForCall;
				++p;
			}

			std::optional<std::string> keyword;
			Expression expr{};

			// Try kwarg
			TokenIter rewind = p;
			if (!p.EndReached() && p->type == Token::Type::Word && unpackType == Operation{}) {
				keyword = p->text;
				++p;
				if (p.EndReached() || p->text != "=") {
					p = rewind;
					keyword.reset();
				} else {
					++p;
					if (auto error = ParseExpression(p, expr)) {
						return error;
					}
				}
			}

			if (keyword.has_value() || unpackType == Operation::UnpackMapForCall) {
				seenKwarg = true;
			} else if (seenKwarg) {
				return CodeError::Bad("Keyword arguments must appear last", rewind->srcPos);
			}
			
			// Get expression
			if (!keyword.has_value()) {
				if (auto error = ParseExpression(p, expr)) {
					return error;
				}
			}

			if (keyword.has_value()) {
				Expression kw{};
				kw.srcPos = expr.srcPos;
				kw.operation = Operation::Kwarg;
				kw.variableName = std::move(keyword.value());
				kw.children.push_back(std::move(expr));
				out.push_back(std::move(kw));
			} else if (unpackType != Operation{}) {
				Expression unpack{};
				unpack.srcPos = expr.srcPos;
				unpack.operation = unpackType;
				unpack.children.push_back(std::move(expr));
				out.push_back(std::move(unpack));
			} else {
				out.push_back(std::move(expr));
			}

			// Check for comma
			if (!p.EndReached() && p->text == ",") {
				if (seenComma) *seenComma = true;
				++p;
			} else {
				mustTerminate = true;
			}
		}
	}

	static CodeError ParsePostfix(TokenIter& p, Expression arg, Expression& out) {
		if (p.EndReached()) {
			out = std::move(arg);
			return CodeError::Good();
		}

		out.srcPos = p->srcPos;
		if (p->text == "++" || p->text == "--") {
			if (!IsAssignableExpression(arg, out.assignTarget)) {
				return CodeError::Bad("Expression is not assignable", (--p)->srcPos);
			}
			Expression one{};
			one.srcPos = out.srcPos;
			one.operation = Operation::Literal;
			one.literalValue.type = LiteralValue::Type::Int;
			one.literalValue.i = 1;

			Expression calc{};
			calc.srcPos = out.srcPos;
			calc.operation = p->text == "++" ? Operation::AddAssign : Operation::SubAssign;
			calc.children.push_back(std::move(arg));
			calc.children.push_back(std::move(one));

			out.operation = Operation::CompoundAssignment;
			out.children.push_back(std::move(calc));
			++p;
		} else if (p->text == "(") {
			// Consume opening bracket
			out.operation = Operation::Call;
			++p;

			// Consume expression list
			out.children.push_back(std::move(arg));
			if (p.EndReached()) {
				return CodeError::Bad("Expected an expression", (--p)->srcPos);
			} else if (auto error = ParseExpressionList(p, ")", out.children, true)) {
				return error;
			}

			// Consume closing bracket
			++p;
		} else if (p->text == "[") {
			// Consume opening bracket
			SourcePosition srcPos = p->srcPos;
			++p;

			std::optional<Expression> indices[3];

			bool isSlice = false;
			for (size_t i = 0; i < std::size(indices); i++) {
				if (p.EndReached()) {
					return CodeError::Bad("Expected an expression", (--p)->srcPos);
				} else if (p->text != ":" && (p->text != "]" || i == 0)) {
					indices[i].emplace();
					if (auto error = ParseExpression(p, indices[i].value())) {
						return error;
					}
				}

				// Consume ']' or ':'
				if (p.EndReached()) {
					return CodeError::Bad("Expected a ']'", (--p)->srcPos);
				} else if (p->text == "]") {
					++p;
					break;
				} else if (p->text != ":" || i == std::size(indices) - 1) {
					return CodeError::Bad("Expected a ']'", p->srcPos);
				}
				isSlice = true;
				++p;
			}

			out.operation = isSlice ? Operation::Slice : Operation::Index;
			out.children.push_back(std::move(arg));
			for (size_t i = 0; i < std::size(indices); i++) {
				if (indices[i].has_value()) {
					out.children.push_back(std::move(indices[i].value()));
				} else if (isSlice) {
					Expression none{};
					none.srcPos = srcPos;
					none.literalValue.type = LiteralValue::Type::Null;
					out.children.push_back(std::move(none));
				}
			}
		} else if (p->text == ".") {
			// Consume dot
			out.operation = Operation::Dot;
			++p;

			// Consume attribute name
			if (p.EndReached()) {
				return CodeError::Bad("Expected an attribute name", (--p)->srcPos);
			} else if (p->type != Token::Type::Word) {
				return CodeError::Bad("Expected an attribute name", p->srcPos);
			}
			out.children.push_back(std::move(arg));
			out.variableName = p->text;
			++p;
		} else if (p->text == "if") {
			// Might as well handle if-else here
			out.operation = Operation::IfElse;

			// Consume 'if'
			TokenIter start = p;
			++p;

			// Consume condition
			Expression condition;
			if (p.EndReached()) {
				return CodeError::Bad("Expected an expression", (--p)->srcPos);
			} else if (auto error = ParseExpression(p, condition, (size_t)0)) {
				return error;
			}

			// Consume 'else'
			if (p.EndReached() || p->text != "else") {
				p = start;
				out = std::move(arg);
				return CodeError::Good();
			}
			++p;

			// Consume false case expression
			Expression falseCase;
			if (p.EndReached()) {
				return CodeError::Bad("Expected an expression", (--p)->srcPos);
			} else if (auto error = ParseExpression(p, falseCase, (size_t)0)) {
				return error;
			}

			out.children.push_back(std::move(condition));
			out.children.push_back(std::move(arg));
			out.children.push_back(std::move(falseCase));
		} else {
			out = std::move(arg);
		}
		return CodeError::Good();
	}

	static CodeError ParseTuple(TokenIter& p, Expression& out) {
		out.srcPos = p->srcPos;
		out.operation = Operation::Tuple;
		++p;

		bool seenComma = false;
		if (p.EndReached()) {
			return CodeError::Bad("Expected an expression", (--p)->srcPos);
		} else if (auto error = ParseExpressionList(p, ")", out.children, false, &seenComma)) {
			return error;
		}
		++p;

		if (!out.children.empty() && !seenComma) {
			// Was just an expression in brackets and not a tuple.
			Expression e = std::move(out.children[0]);
			out = std::move(e);
		}

		return CodeError::Good();
	}

	static CodeError ParseList(TokenIter& p, Expression& out) {
		out.srcPos = p->srcPos;
		out.operation = Operation::List;
		++p;

		if (p.EndReached()) {
			return CodeError::Bad("Expected an expression", (--p)->srcPos);
		} else if (auto error = ParseExpressionList(p, "]", out.children)) {
			return error;
		}

		++p;

		return CodeError::Good();
	}

	static CodeError ParseSet(TokenIter& p, Expression& out) {
		out.srcPos = p->srcPos;
		out.operation = Operation::Set;
		++p;

		if (p.EndReached()) {
			return CodeError::Bad("Expected an expression", (--p)->srcPos);
		} else if (auto error = ParseExpressionList(p, "}", out.children)) {
			return error;
		}

		++p;

		return CodeError::Good();
	}

	static CodeError ParseMap(TokenIter& p, Expression& out) {
		out.srcPos = p->srcPos;
		out.operation = Operation::Map;
		++p;
		bool mustTerminate = false;
		while (true) {
			// Check for terminating token
			if (p.EndReached()) {
				return CodeError::Bad("Expected a closing bracket", (--p)->srcPos);
			} else if (p->text == "}") {
				++p;
				return CodeError::Good();
			} else if (mustTerminate) {
				return CodeError::Bad("Expected a closing bracket", p->srcPos);
			}

			if (p->text == "**") {
				// Unpack map
				Expression unpack{};
				unpack.srcPos = p->srcPos;
				unpack.operation = Operation::UnpackMapForMapCreation;
				++p;

				Expression map{};
				if (p.EndReached()) {
					return CodeError::Bad("Expected a closing bracket", (--p)->srcPos);
				} else if (auto error = ParseExpression(p, map)) {
					return error;
				}
				
				unpack.children.push_back(std::move(map));
				out.children.push_back(std::move(unpack));
			} else {
				// Get key
				Expression key{};
				if (auto error = ParseExpression(p, key)) {
					return error;
				}
				out.children.push_back(std::move(key));

				// Check for colon
				if (p.EndReached()) {
					return CodeError::Bad("Expected a ':'", (--p)->srcPos);
				} else if (p->text != ":") {
					return CodeError::Bad("Expected a ':'", p->srcPos);
				}
				++p;

				// Get value
				Expression value{};
				if (auto error = ParseExpression(p, value)) {
					return error;
				}
				out.children.push_back(std::move(value));
			}

			// Check for comma
			if (!p.EndReached() && p->text == ",") {
				++p;
			} else {
				mustTerminate = true;
			}
		}
	}

	static CodeError TryParseListComprehension(TokenIter& p, Expression& out, bool& isListComp) {
		isListComp = false;
		out.srcPos = p->srcPos;
		out.operation = Operation::ListComprehension;
		TokenIter begin = p;
		++p;

		Expression value{};
		if (auto error = ParseExpression(p, value)) {
			p = begin;
			return CodeError::Good();
		}

		if (p.EndReached()) {
			p = begin;
			return CodeError::Good();
		} else if (p->text != "for") {
			p = begin;
			return CodeError::Good();
		}
		isListComp = true;
		++p;

		std::vector<std::string> vars;
		bool isTuple{};
		if (auto error = ParseForLoopVariableList(p, vars, isTuple)) {
			return error;
		}

		AssignTarget assignTarget{};
		if (!isTuple) {
			assignTarget.type = AssignType::Direct;
			assignTarget.direct = vars[0];
		} else {
			assignTarget.type = AssignType::Pack;
			for (auto& var : vars) {
				AssignTarget elem{};
				elem.type = AssignType::Direct;
				elem.direct = std::move(var);
				assignTarget.pack.push_back(std::move(elem));
			}
		}
		++p;

		Expression iterable{};
		if (auto error = ParseExpression(p, iterable)) {
			return error;
		}

		Expression condition{};
		if (p.EndReached()) {
			return CodeError::Bad("Expected a ']'", (--p)->srcPos);
		} else if (p->text == "if") {
			++p;
			if (auto error = ParseExpression(p, condition)) {
				return error;
			}
		} else {
			condition.srcPos = p->srcPos;
			condition.operation = Operation::Literal;
			condition.literalValue.type = LiteralValue::Type::Bool;
			condition.literalValue.b = true;
		}

		if (p.EndReached()) {
			return CodeError::Bad("Expected a ']'", (--p)->srcPos);
		} else if (p->text != "]") {
			return CodeError::Bad("Expected a ']'", p->srcPos);
		}
		++p;

		std::string listName = "__ListComp" + std::to_string(Guid());

		Expression loadList{};
		loadList.srcPos = out.srcPos;
		loadList.operation = Operation::Variable;
		loadList.variableName = listName;
		
		Expression append{};
		append.srcPos = out.srcPos;
		append.operation = Operation::Dot;
		append.children.push_back(std::move(loadList));
		append.variableName = "append";

		Expression appendCall{};
		appendCall.srcPos = out.srcPos;
		appendCall.operation = Operation::Call;
		appendCall.children.push_back(std::move(append));
		appendCall.children.push_back(std::move(value));

		Statement appendStat{};
		appendStat.srcPos = out.srcPos;
		appendStat.type = Statement::Type::Expr;
		appendStat.expr = std::move(appendCall);

		Statement ifStat{};
		ifStat.srcPos = out.srcPos;
		ifStat.type = Statement::Type::If;
		ifStat.expr = std::move(condition);
		ifStat.body.push_back(std::move(appendStat));

		Statement forLoop{};
		forLoop.srcPos = out.srcPos;
		forLoop.type = Statement::Type::For;
		forLoop.forLoop.assignTarget = std::move(assignTarget);
		forLoop.expr = std::move(iterable);
		forLoop.body.push_back(std::move(ifStat));

		out.listComp.listName = listName;
		out.listComp.forBody.push_back(TransformForToWhile(std::move(forLoop)));
		ExpandCompositeStatements(out.listComp.forBody);
		
		return CodeError::Good();
	}

	static CodeError ParseLambda(TokenIter& p, Expression& out) {
		out.srcPos = p->srcPos;
		++p;

		std::vector<Parameter> params;
		if (p.EndReached()) {
			return CodeError::Bad("Expected a ':'", (--p)->srcPos);
		} else if (auto error = ParseParameterList(p, params)) {
			return error;
		}

		if (p.EndReached()) {
			return CodeError::Bad("Expected a ':'", (--p)->srcPos);
		} else if (p->text != ":") {
			return CodeError::Bad("Expected a ':'", p->srcPos);
		}

		++p;
		Expression lambdaExpr{};
		if (auto error = ParseExpression(p, lambdaExpr)) {
			return error;
		}

		auto captures = GetReferencedVariables(lambdaExpr);
		for (const auto& param : params)
			captures.erase(param.name);

		Statement lambdaRet{};
		lambdaRet.srcPos = out.srcPos;
		lambdaRet.type = Statement::Type::Return;
		lambdaRet.expr = std::move(lambdaExpr);

		out.operation = Operation::Function;
		out.def.localCaptures = std::move(captures);
		out.def.name = "<lambda>";
		out.def.parameters = std::move(params);
		out.def.body.push_back(std::move(lambdaRet));

		return CodeError::Good();
	}

	static CodeError ParseValue(TokenIter& p, Expression& out) {
		// Parse standalone values
		out = {};
		if (p->text == "(") {
			if (auto error = ParseTuple(p, out)) {
				return error;
			}
		} else if (p->text == "[") {
			bool isListComprehension = false;
			if (auto error = TryParseListComprehension(p, out, isListComprehension)) {
				return error;
			} else if (isListComprehension) {
				// Do nothing
			} else if (auto error = ParseList(p, out)) {
				return error;
			}
		} else if (p->text == "{") {
			TokenIter start = p;
			if (ParseSet(p, out)) {
				// Not a valid set, so expect a map
				p = start;
				out = {};
				if (auto error = ParseMap(p, out)) {
					return error;
				}
			} else if (out.children.empty()) {
				// Found empty set but this should actually be a dictionary
				out.operation = Operation::Map;
			}
		} else if (p->text == "lambda") {
			if (auto error = ParseLambda(p, out)) {
				return error;
			}
		} else {
			switch (p->type) {
			case Token::Type::Null:
				out.literalValue.type = LiteralValue::Type::Null;
				break;
			case Token::Type::Bool:
				out.literalValue.type = LiteralValue::Type::Bool;
				out.literalValue.b = p->literal.b;
				break;
			case Token::Type::Int:
				out.literalValue.type = LiteralValue::Type::Int;
				out.literalValue.i = p->literal.i;
				break;
			case Token::Type::Float:
				out.literalValue.type = LiteralValue::Type::Float;
				out.literalValue.f = p->literal.f;
				break;
			case Token::Type::String:
				out.literalValue.type = LiteralValue::Type::String;
				out.literalValue.s = p->literal.s;
				break;
			case Token::Type::Word:
				out.operation = Operation::Variable;
				out.variableName = p->text;
				break;
			default:
				return CodeError::Bad("Unexpected token", p->srcPos);
			}
			out.srcPos = p->srcPos;
			++p;
		}

		// Apply any postfix operators
		TokenIter oldP = p;
		do {
			Expression operand = std::move(out);
			out = {};
			oldP = p;
			if (auto error = ParsePostfix(p, std::move(operand), out)) {
				return error;
			}
		} while (oldP != p);

		return CodeError::Good();
	}

	static CodeError ParsePrefix(TokenIter& p, Expression& out) {
		if (PREFIX_UNARY_OP_STRINGS.contains(p->text)) {
			Operation op = PREFIX_UNARY_OP_STRINGS.at(p->text);
			out.srcPos = p->srcPos;
			++p;
			if (p.EndReached()) {
				return CodeError::Bad("Expected an expression", (--p)->srcPos);
			}
			out.operation = op;
			out.children.emplace_back();
			return ParsePrefix(p, out.children[0]);
		} else {
			return ParseValue(p, out);
		}
	}

	static CodeError ParseExpression(TokenIter& p, Expression& out, size_t minPrecedence, std::optional<Expression> preParsedArg) {
		Expression lhs{};
		if (preParsedArg.has_value()) {
			lhs = std::move(preParsedArg.value());
		} else {
			if (auto error = ParsePrefix(p, lhs)) {
				return error;
			}
		}

		if (p.EndReached() || !BINARY_OP_STRINGS.contains(p->text)) {
			out = std::move(lhs);
			return CodeError::Good();
		}
		Operation op = BINARY_OP_STRINGS.at(p->text);
		size_t precedence = PrecedenceOf(op);
		if (precedence < minPrecedence) {
			out = std::move(lhs);
			return CodeError::Good();
		} else if (op == Operation::NotIn) {
			// 'not in' is a special case since it contains 2 tokens
			++p;
			if (p.EndReached()) {
				return CodeError::Bad("Expected a 'in'", (--p)->srcPos);
			} else if (p->text != "in") {
				return CodeError::Bad("Expected a 'in'", p->srcPos);
			}
		} else if (disableInOperator && op == Operation::In) {
			out = std::move(lhs);
			return CodeError::Good();
		}
		++p;

		if (op == Operation::Is && !p.EndReached() && p->text == "not") {
			op = Operation::IsNot;
			++p;
		}

		if (p.EndReached()) {
			return CodeError::Bad("Expected an expression", (--p)->srcPos);
		}		
		out.srcPos = p->srcPos;
		if (BINARY_RIGHT_ASSOCIATIVE_OPS.contains(op)) {
			// Binary operation is an assignment operation only if it is right associative
			if (!IsAssignableExpression(lhs, out.assignTarget)) {
				return CodeError::Bad("Expression is not assignable", (----p)->srcPos);
			}

			Expression rhs{};
			if (auto error = ParseExpression(p, rhs)) {
				return error;
			}

			if (op != Operation::Assign) {
				// Compound assignment
				Expression calc{};
				calc.srcPos = out.srcPos;
				calc.operation = op;
				calc.children.push_back(std::move(lhs));
				calc.children.push_back(std::move(rhs));

				out.operation = Operation::CompoundAssignment;
				out.children.push_back(std::move(calc));
			} else {
				out.operation = op;
				out.children.push_back(std::move(lhs));
				out.children.push_back(std::move(rhs));
			}
			return CodeError::Good();
		} else {
			Expression rhs{};
			if (auto error = ParseExpression(p, rhs, precedence + 1)) {
				return error;
			}
			out.operation = op;
			out.children.push_back(std::move(lhs));
			out.children.push_back(std::move(rhs));

			TokenIter oldP = p;
			do {
				lhs = std::move(out);
				out = {};
				oldP = p;
				if (auto error = ParseExpression(p, out, minPrecedence + 1, std::move(lhs))) {
					return error;
				}
			} while (oldP != p);
			return CodeError::Good();
		}
	}

	CodeError ParseExpression(TokenIter& p, Expression& out, bool disableInOp) {
		disableInOperator = disableInOp;
		if (p.EndReached()) {
			return CodeError::Bad("Expected an expression", (--p)->srcPos);
		} else {
			return ParseExpression(p, out, (size_t)0);
		}
	}
}


#include <regex>
#include <optional>
#include <stack>

namespace wings {

	std::string Token::ToString() const {
		std::vector<std::pair<std::string, std::string>> props;

		props.push_back({ "text", '"' + text + '"' });
		props.push_back({ "srcPos", '(' + std::to_string(srcPos.line + 1)
								  + ',' + std::to_string(srcPos.column + 1) + ')' });
		switch (type) {
		case Token::Type::Null:
			props.push_back({ "type", "null" });
			break;
		case Token::Type::Bool:
			props.push_back({ "type", "bool" });
			props.push_back({ "value", literal.b ? "True" : "False" });
			break;
		case Token::Type::Int:
			props.push_back({ "type", "int" });
			props.push_back({ "value", std::to_string(literal.i) });
			break;
		case Token::Type::Float:
			props.push_back({ "type", "float" });
			props.push_back({ "value", std::to_string(literal.f) });
			break;
		case Token::Type::String:
			props.push_back({ "type", "string" });
			props.push_back({ "value", literal.s });
			break;
		case Token::Type::Symbol:
			props.push_back({ "type", "symbol" });
			break;
		case Token::Type::Word:
			props.push_back({ "type", "word" });
			break;
		default:
			WG_UNREACHABLE();
		}

		std::string s = "{ ";
		for (const auto& p : props)
			s += p.first + ": " + p.second + ", ";
		return s + "}";
	}

	static const std::vector<std::string> SYMBOLS = {
		"(", ")", "[", "]", "{", "}", ":", ".", ",",
		"+", "-", "*", "**", "/", "//", "%",
		"<", ">", "<=", ">=", "==", "!=",
		"!", "&&", "||", "^", "&", "|", "~",
		"=", ":=",
		"+=", "-=", "*=", "**=", "%=", "/=", "//=",
		">>=", "<<=", "|=", "&=", "^=", ";", "--", "++"
	};

	static std::string NormalizeLineEndings(const std::string& text) {
		auto s = std::regex_replace(text, std::regex("\r\n"), "\n");
		std::replace(s.begin(), s.end(), '\r', '\n');
		return s;
	}

	static bool IsAlpha(char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
	}

	static bool IsDigit(char c, int base = 10) {
		switch (base) {
		case 2: return c >= '0' && c <= '1';
		case 8: return c >= '0' && c <= '7';
		case 10: return c >= '0' && c <= '9';
		case 16: return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
		default: WG_UNREACHABLE();
		}
	}

	static int DigitValueOf(char c, int base) {
		switch (base) {
		case 2:
		case 8:
		case 10:
			return c - '0';
		case 16:
			if (c >= '0' && c <= '9') {
				return c - '0';
			} else if (c >= 'a' && c <= 'f') {
				return c - 'a' + 10;
			} else {
				return c - 'A' + 10;
			}
		default:
			WG_UNREACHABLE();
		}
	}

	static bool IsAlphaNum(char c) {
		return IsAlpha(c) || IsDigit(c);
	}

	static bool IsWhitespace(const std::string& s) {
		return s.find_first_not_of(" \t") == std::string::npos;
	}

	static bool IsWhitespaceChar(char c) {
		return c == ' ' || c == '\t';
	}

	static void StripComments(std::string& s) {
		s.erase(
			std::find(s.begin(), s.end(), '#'),
			s.end()
		);
	}

	static bool IsPossibleSymbol(const std::string& s) {
		return std::any_of(SYMBOLS.begin(), SYMBOLS.end(), [&](const auto& x) { return x.starts_with(s); });
	}

	static bool IsPossibleSymbol(char c) {
		return IsPossibleSymbol(std::string(1, c));
	}

	static std::vector<std::string> SplitLines(const std::string& s) {
		std::vector<std::string> v;
		size_t last = 0;
		size_t next = 0;
		while ((next = s.find('\n', last)) != std::string::npos) {
			v.push_back(s.substr(last, next - last));
			last = next + 1;
		}
		v.push_back(s.substr(last));
		return v;
	}

	static int IndentOf(const std::string& line, std::optional<std::string>& indentString, size_t& indent) {
		size_t i = 0;
		while (true) {
			// Reached end of line or comment before any code
			if (i >= line.size() || line[i] == '#') {
				indent = 0;
				return 0;
			}

			// Reached code
			if (line[i] != ' ' && line[i] != '\t')
				break;

			i++;
		}

		if (i == 0) {
			// No indent
			indent = 0;
			return 0;
		} else {
			// Make sure indent is either all spaces or all tabs
			if (!std::all_of(line.begin(), line.begin() + i, [&](char c) { return c == line[0]; })) {
				return -1;
			}

			if (!indentString.has_value()) {
				// Encountered first indent
				indentString = line.substr(0, i);
				indent = 1;
				return 0;
			} else {
				// Make sure indent is consistent with previous indents
				if (i % indentString.value().size()) {
					return -1;
				}

				indent = i / indentString.value().size();
				return 0;
			}
		}
	}

	using StringIter = const char*;

	static Token ConsumeWord(StringIter& p) {
		Token t{};
		for (; *p && IsAlphaNum(*p); ++p) {
			t.text += *p;
		}
		t.type = Token::Type::Word;
		if (t.text == "None") {
			t.type = Token::Type::Null;
		} else if (t.text == "True" || t.text == "False") {
			t.type = Token::Type::Bool;
			t.literal.b = t.text[0] == 'T';
		} else if (IsKeyword(t.text)) {
			t.type = Token::Type::Keyword;
		}
		return t;
	}

	static CodeError ConsumeNumber(StringIter& p, Token& out) {
		StringIter start = p;

		Token t{};
		int base = 10;
		if (*p == '0') {
			switch (p[1]) {
			case 'b': case 'B': base = 2; break;
			case 'o': case 'O': base = 8; break;
			case 'x': case 'X': base = 16; break;
			}
		}

		if (base != 10) {
			t.text += p[0];
			t.text += p[1];
			p += 2;

			if (!IsDigit(*p, base) && *p != '.') {
				switch (base) {
				case 2: return CodeError::Bad("Invalid binary string");
				case 8: return CodeError::Bad("Invalid octal string");
				case 16: return CodeError::Bad("Invalid hexadecimal string");
				default: WG_UNREACHABLE();
				}
			}
		}

		uintmax_t value = 0;
		for (; *p && IsDigit(*p, base); ++p) {
			value = (base * value) + DigitValueOf(*p, base);
		}

		if (*p == '.') {
			// Is a float
			++p;
			Wg_float fvalue = (Wg_float)value;
			for (int i = 1; *p && IsDigit(*p, base); ++p, ++i) {
				fvalue += DigitValueOf(*p, base) * std::pow((Wg_float)base, (Wg_float)-i);
			}
			t.literal.f = fvalue;
			t.type = Token::Type::Float;
		} else {
			// Is an int
			if (value > std::numeric_limits<Wg_uint>::max()) {
				return CodeError::Bad("Integer literal is too large");
			}
			Wg_uint u = (Wg_uint)value;
			static_assert(sizeof(t.literal.i) == sizeof(u));
			std::memcpy(&t.literal.i, &u, sizeof(u));
			t.type = Token::Type::Int;
		}

		if (IsAlphaNum(*p)) {
			return CodeError::Bad("Invalid numerical literal");
		}

		t.text = std::string(start, p);
		out = std::move(t);
		return CodeError::Good();
	}

	static bool IsHexDigit(char c, int& val) {
		if (c >= '0' && c <= '9') {
			val = c - '0';
			return true;
		} else if (c >= 'a' && c <= 'f') {
			val = c - 'a' + 10;
			return true;
		} else if (c >= 'A' && c <= 'F') {
			val = c - 'A' + 10;
			return true;
		} else {
			return false;
		}
	}

	static CodeError ConsumeString(StringIter& p, Token& out) {
		char quote = *p;
		++p;

		Token t{};
		for (; *p && *p != quote; ++p) {
			t.text += *p;

			// Escape sequences
			if (*p == '\\') {
				++p;
				if (*p == '\0') {
					return CodeError::Bad("Missing closing quote");
				}

				if (*p == 'x') {
					++p;
					int	d1 = 0;
					if (!IsHexDigit(*p, d1)) {
						return CodeError::Bad("Invalid hex escape sequence");
					}
					t.text += *p;
					
					++p;
					int d2 = 0;
					if (!IsHexDigit(*p, d2)) {
						return CodeError::Bad("Invalid hex escape sequence");
					}
					t.text += *p;
					
					t.literal.s += (char)((d1 << 4) | d2);
				} else {
					char esc = 0;
					switch (*p) {
					case '0': esc = '\0'; break;
					case 'n': esc = '\n'; break;
					case 'r': esc = '\r'; break;
					case 't': esc = '\t'; break;
					case 'v': esc = '\v'; break;
					case 'b': esc = '\b'; break;
					case 'f': esc = '\f'; break;
					case '"': esc = '"'; break;
					case '\'': esc = '\''; break;
					case '\\': esc = '\\'; break;
					default: return CodeError::Bad("Invalid escape sequence");
					}
					t.text += *p;
					t.literal.s += esc;
				}
			} else {
				t.literal.s += *p;
			}
		}

		if (*p == '\0') {
			return CodeError::Bad("Missing closing quote");
		}

		// Skip closing quote
		++p;

		t.text = quote + t.text + quote;
		t.type = Token::Type::String;
		out = std::move(t);
		return CodeError::Good();
	}

	static void ConsumeWhitespace(StringIter& p) {
		while (*p && IsWhitespaceChar(*p))
			++p;
	}

	static CodeError ConsumeSymbol(StringIter& p, Token& t) {
		for (; *p && IsPossibleSymbol(t.text + *p); ++p) {
			t.text += *p;
		}
		t.type = Token::Type::Symbol;

		if (std::find(SYMBOLS.begin(), SYMBOLS.end(), t.text) == SYMBOLS.end()) {
			return CodeError::Bad("Unrecognised symbol " + t.text);
		} else {
			return CodeError::Good();
		}
	}

	static CodeError TokenizeLine(const std::string& line, std::vector<Token>& out) {
		std::vector<Token> tokens;
		CodeError error = CodeError::Good();

		StringIter p = line.data();
		while (*p) {
			size_t srcColumn = p - line.data();
			bool wasWhitespace = false;

			if (IsAlpha(*p)) {
				tokens.push_back(ConsumeWord(p));
			} else if (IsDigit(*p)) {
				Token t{};
				if (!(error = ConsumeNumber(p, t))) {
					tokens.push_back(std::move(t));
				}
			} else if (*p == '\'' || *p == '"') {
				Token t{};
				if (!(error = ConsumeString(p, t))) {
					tokens.push_back(std::move(t));
				}
			} else if (IsPossibleSymbol(*p)) {
				Token t{};
				if (!(error = ConsumeSymbol(p, t))) {
					tokens.push_back(std::move(t));
				}
			} else if (IsWhitespaceChar(*p)) {
				ConsumeWhitespace(p);
				wasWhitespace = true;
			} else {
				error.good = false;
				error.srcPos.column = srcColumn;
				error.message = std::string("Unrecognised character ") + *p;
			}

			if (error) {
				out.clear();
				error.srcPos.column = srcColumn;
				return error;
			}

			if (!wasWhitespace) {
				tokens.back().srcPos.column = srcColumn;
			}
		}

		out = std::move(tokens);
		return CodeError::Good();
	}

	// Returns [no. of open brackets] minus [no. close brackets]
	static int BracketBalance(std::vector<Token>& tokens) {
		int balance = 0;
		for (const auto& t : tokens) {
			if (t.text.size() == 1) {
				switch (t.text[0]) {
				case '(': case '[': case '{': balance++; break;
				case ')': case ']': case '}': balance--; break;
				}
			}
		}
		return balance;
	}

	LexResult Lex(std::string code) {
		code = NormalizeLineEndings(code);
		auto originalSource = SplitLines(code);

		std::vector<std::string> lines = originalSource;
		for (auto& line : lines)
			StripComments(line);

		CodeError error = CodeError::Good();
		std::optional<std::string> indentString;
		int bracketBalance = 0;

		LexTree rootTree;
		std::stack<LexTree*> parents;
		parents.push(&rootTree);

		for (size_t i = 0; i < lines.size(); i++) {
			if (IsWhitespace(lines[i]))
				continue;

			std::vector<Token> tokens;
			if (error = TokenizeLine(lines[i], tokens)) {
				// Line had tokenizing errors
				error.srcPos.line = i;
				break;
			} else {
				// Assign line numbers
				for (auto& token : tokens) {
					token.srcPos.line = i;
				}
			}

			bool continuePrevLine = bracketBalance > 0;
			bracketBalance = std::max(0, bracketBalance + BracketBalance(tokens));
			if (continuePrevLine) {
				// Ignore indenting and continue as previous line
				auto& prevLineTokens = parents.top()->children.back().tokens;
				prevLineTokens.insert(prevLineTokens.end(), tokens.begin(), tokens.end());
				continue;
			}

			// Get indentation level
			size_t parentIndent = parents.size() - 1;
			size_t currentIndent = 0;
			if (IndentOf(lines[i], indentString, currentIndent)) {
				error = CodeError::Bad("Invalid indentation", { i, 0 });
				break;
			}

			if (currentIndent > parentIndent + 1) {
				// Indented too much
				error = CodeError::Bad("Indentation level increased by more than 1", { i, 0 });
				break;
			} else if (currentIndent == parentIndent + 1) {
				// Indented
				// Make the last child the new parent
				if (parents.top()->children.empty()) {
					error = CodeError::Bad("Indentation not expected", { i, 0 });
					break;
				}
				parents.push(&parents.top()->children.back());
			} else if (currentIndent < parentIndent) {
				// De-indented
				for (size_t j = 0; j < parentIndent - currentIndent; j++)
					parents.pop();
			}

			parents.top()->children.push_back(LexTree{ std::move(tokens), {} });
		}

		LexResult result{};
		result.error = std::move(error);
		result.lexTree = std::move(rootTree);
		result.originalSource = std::move(originalSource);
		return result;
	}
}


namespace wings {
	bool ImportMath(Wg_Context* context);
}


#include <cmath>
#include <numbers>
#include <limits>

namespace wings {
	namespace mathmodule {
		static constexpr const char* MATH_CODE = R"(
def comb(n, k):
	if not isinstance(n, int) or not isinstance(k, int):
		raise TypeError("comb() only accepts integers")
	if n < 0 or k < 0:
		raise ValueError("comb() only accepts non-negative integers")
	if k > n:
		return 0
	return factorial(n) // (factorial(k) * factorial(n - k))

def fabs(x):
	return float(abs(x))

def factorial(n):
	if not isinstance(n, int):
		raise TypeError("factorial() only accepts integers")
	if n < 0:
		raise ValueError("factorial() only accepts non-negative integers")
	if n == 0:
		return 1
	return n * factorial(n - 1)

def gcd(*integers):
	if len(integers) == 0:
		raise TypeError("gcd() requires at least one argument")
	for i in integers:
		if not isinstance(i, int):
			raise TypeError("gcd() only accepts integers")
	if len(integers) == 1:
		return abs(integers[0])
	if len(integers) == 2:
		(a, b) = integers
		if a == 0:
			return abs(b)
		if b == 0:
			return abs(a)
		while b != 0:
			(a, b) = (b, a % b)
		return abs(a)
	return gcd(gcd(integers[0], integers[1]), *integers[2:])

def lcm(*integers):
	if len(integers) == 0:
		raise TypeError("lcm() requires at least one argument")
	for i in integers:
		if not isinstance(i, int):
			raise TypeError("lcm() only accepts integers")
	if len(integers) == 1:
		return abs(integers[0])
	if len(integers) == 2:
		(a, b) = integers
		if a == 0 or b == 0:
			return 0
		return abs(a * b) // gcd(a, b)
	return lcm(lcm(integers[0], integers[1]), *integers[2:])

def modf(x):
	r = x % 1.0
	return (r, x - r)

def perm(n, k=None):
	if not isinstance(n, int):
		raise TypeError("perm() only accepts integers")
	if n < 0:
		raise ValueError("perm() only accepts non-negative integers")
	if k is None:
		k = n
	if not isinstance(k, int):
		raise TypeError("perm() only accepts integers")
	if k < 0:
		raise ValueError("perm() only accepts non-negative integers")
	if k > n:
		return 0
	return factorial(n) // factorial(n - k)

def trunc(x):
	if x >= 0:
		return int(x)
	return int(x) - 1

def exp(x):
	return e ** x

def log1p(x):
	return log(1 + x)

def log2(x):
	return log(x, 2)

def log10(x):
	return log(x, 10)

def pow(x, y):
	if x == 1 or y == 0:
		return 1
	if isfinite(x) and isfinite(y) and x < 0 and isinstance(y, int):
		raise ValueError("negative number cannot be raised to a fractional power")
	return x ** y

def sqrt(x):
	return x ** 0.5

def dist(p, q):
	return sqrt(sum([(z[0] - z[1]) ** 2 for z in zip(p, q)]))

def hypot(*coords):
	return sqrt(sum([x ** 2 for x in coords]))

def degrees(x):
	return x * 180.0 / pi

def radians(x):
	return x * pi / 180.0
)";

		static Wg_Obj* ceil(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			if (Wg_IsIntOrFloat(argv[0])) {
				return Wg_NewInt(context, (Wg_int)std::ceil(Wg_GetFloat(argv[0])));
			}
			return Wg_CallMethod(argv[0], "__ceil__", nullptr, 0);
		}

		static Wg_Obj* floor(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			if (Wg_IsIntOrFloat(argv[0])) {
				return Wg_NewInt(context, (Wg_int)std::floor(Wg_GetFloat(argv[0])));
			}
			return Wg_CallMethod(argv[0], "__floor__", nullptr, 0);
		}

		using FpCheck = bool(*)(Wg_float);

		template <FpCheck f>
		static Wg_Obj* isx(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			return Wg_NewBool(context, f(Wg_GetFloat(argv[0])));
		}

		static Wg_Obj* isfinite(Wg_Context* context, Wg_Obj** argv, int argc) {
			return isx<std::isfinite>(context, argv, argc);
		}

		static Wg_Obj* isinf(Wg_Context* context, Wg_Obj** argv, int argc) {
			return isx<std::isinf>(context, argv, argc);
		}

		static Wg_Obj* isnan(Wg_Context* context, Wg_Obj** argv, int argc) {
			return isx<std::isnan>(context, argv, argc);
		}

		static Wg_Obj* log(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 2);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			Wg_float base = std::numbers::e_v<Wg_float>;
			if (argc == 2) {
				WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
				base = Wg_GetFloat(argv[1]);
			}
			return Wg_NewFloat(context, std::log(Wg_GetFloat(argv[0])) / std::log(base));
		}

		using Op = Wg_float(*)(Wg_float);

		template <Op op>
		static Wg_Obj* opx(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			return Wg_NewFloat(context, op(Wg_GetFloat(argv[0])));
		}

		static Wg_Obj* cos(Wg_Context* context, Wg_Obj** argv, int argc) {
			return opx<std::cos>(context, argv, argc);
		}

		static Wg_Obj* sin(Wg_Context* context, Wg_Obj** argv, int argc) {
			return opx<std::sin>(context, argv, argc);
		}

		static Wg_Obj* tan(Wg_Context* context, Wg_Obj** argv, int argc) {
			return opx<std::tan>(context, argv, argc);
		}

		static Wg_Obj* acos(Wg_Context* context, Wg_Obj** argv, int argc) {
			return opx<std::acos>(context, argv, argc);
		}

		static Wg_Obj* asin(Wg_Context* context, Wg_Obj** argv, int argc) {
			return opx<std::asin>(context, argv, argc);
		}

		static Wg_Obj* atan(Wg_Context* context, Wg_Obj** argv, int argc) {
			return opx<std::atan>(context, argv, argc);
		}

		static Wg_Obj* cosh(Wg_Context* context, Wg_Obj** argv, int argc) {
			return opx<std::cosh>(context, argv, argc);
		}

		static Wg_Obj* sinh(Wg_Context* context, Wg_Obj** argv, int argc) {
			return opx<std::sinh>(context, argv, argc);
		}

		static Wg_Obj* tanh(Wg_Context* context, Wg_Obj** argv, int argc) {
			return opx<std::tanh>(context, argv, argc);
		}

		static Wg_Obj* acosh(Wg_Context* context, Wg_Obj** argv, int argc) {
			return opx<std::acosh>(context, argv, argc);
		}

		static Wg_Obj* asinh(Wg_Context* context, Wg_Obj** argv, int argc) {
			return opx<std::asinh>(context, argv, argc);
		}

		static Wg_Obj* atanh(Wg_Context* context, Wg_Obj** argv, int argc) {
			return opx<std::atanh>(context, argv, argc);
		}

		static Wg_Obj* erf(Wg_Context* context, Wg_Obj** argv, int argc) {
			return opx<std::erf>(context, argv, argc);
		}

		static Wg_Obj* erfc(Wg_Context* context, Wg_Obj** argv, int argc) {
			return opx<std::erfc>(context, argv, argc);
		}

		static Wg_Obj* gamma(Wg_Context* context, Wg_Obj** argv, int argc) {
			return opx<std::tgamma>(context, argv, argc);
		}

		static Wg_Obj* lgamma(Wg_Context* context, Wg_Obj** argv, int argc) {
			return opx<std::lgamma>(context, argv, argc);
		}

		static Wg_Obj* atan2(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return Wg_NewFloat(context, std::atan2(Wg_GetFloat(argv[0]), Wg_GetFloat(argv[1])));
		}
	}
	
	bool ImportMath(Wg_Context* context) {
		using namespace mathmodule;
		try {
			RegisterFunction(context, "ceil", ceil);
			RegisterFunction(context, "floor", floor);
			RegisterFunction(context, "isfinite", isfinite);
			RegisterFunction(context, "isinf", isinf);
			RegisterFunction(context, "isnan", isnan);
			RegisterFunction(context, "log", log);
			RegisterFunction(context, "cos", cos);
			RegisterFunction(context, "sin", sin);
			RegisterFunction(context, "tan", tan);
			RegisterFunction(context, "acos", acos);
			RegisterFunction(context, "asin", asin);
			RegisterFunction(context, "atan", atan);
			RegisterFunction(context, "atan2", atan2);
			RegisterFunction(context, "cosh", cosh);
			RegisterFunction(context, "sinh", sinh);
			RegisterFunction(context, "tanh", tanh);
			RegisterFunction(context, "acosh", acosh);
			RegisterFunction(context, "asinh", asinh);
			RegisterFunction(context, "atanh", atanh);
			RegisterFunction(context, "erf", erf);
			RegisterFunction(context, "erfc", erfc);
			RegisterFunction(context, "gamma", gamma);
			RegisterFunction(context, "lgamma", lgamma);

			RegisterConstant(context, "e", Wg_NewFloat, std::numbers::e_v<Wg_float>);
			RegisterConstant(context, "inf", Wg_NewFloat, std::numeric_limits<Wg_float>::infinity());
			RegisterConstant(context, "nan", Wg_NewFloat, std::numeric_limits<Wg_float>::quiet_NaN());
			RegisterConstant(context, "pi", Wg_NewFloat, std::numbers::pi_v<Wg_float>);
			RegisterConstant(context, "tau", Wg_NewFloat, 2 * std::numbers::pi_v<Wg_float>);

			if (Execute(context, MATH_CODE, "math") == nullptr)
				throw LibraryInitException();

			return true;
		} catch (LibraryInitException&) {
			return false;
		}
	}
}


namespace wings {
	bool ImportOS(Wg_Context* context);
}


#include <cstdlib>
#include <filesystem>

namespace wings {
	namespace osmodule {
		namespace fs = std::filesystem;

		static Wg_Obj* system(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_STRING(0);
			int ec = std::system(Wg_GetString(argv[0]));
			return Wg_NewInt(context, (Wg_int)ec);
		}

		static Wg_Obj* mkdir(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_STRING(0);
			const char* path = Wg_GetString(argv[0]);
			std::error_code ec{};
			if (!fs::create_directory(path, ec)) {
				Wg_RaiseException(context, WG_EXC_OSERROR);
				return nullptr;
			}
			return Wg_None(context);
		}

		static Wg_Obj* makedirs(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_STRING(0);
			const char* path = Wg_GetString(argv[0]);
			std::error_code ec{};
			if (!fs::create_directories(path, ec)) {
				Wg_RaiseException(context, WG_EXC_OSERROR);
				return nullptr;
			}
			return Wg_None(context);
		}

		static Wg_Obj* remove(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_STRING(0);
			const char* path = Wg_GetString(argv[0]);

			std::error_code ec{};
			if (!fs::is_regular_file(path, ec)) {
				Wg_RaiseException(context, WG_EXC_ISADIRECTORYERROR);
				return nullptr;
			}

			if (!fs::remove(path, ec)) {
				Wg_RaiseException(context, WG_EXC_OSERROR);
				return nullptr;
			}

			return Wg_None(context);
		}

		static Wg_Obj* rmdir(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_STRING(0);
			const char* path = Wg_GetString(argv[0]);

			std::error_code ec{};
			if (!fs::is_directory(path, ec)) {
				Wg_RaiseException(context, WG_EXC_OSERROR);
				return nullptr;
			}

			if (!fs::remove(path, ec)) {
				Wg_RaiseException(context, WG_EXC_OSERROR);
				return nullptr;
			}

			return Wg_None(context);
		}

		static Wg_Obj* rename(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_STRING(0);
			WG_EXPECT_ARG_TYPE_STRING(1);
			const char* src = Wg_GetString(argv[0]);
			const char* dst = Wg_GetString(argv[1]);

			std::error_code ec{};
			fs::rename(src, dst, ec);
			if (ec) {
				Wg_RaiseException(context, WG_EXC_OSERROR);
				return nullptr;
			}

			return Wg_None(context);
		}

		static Wg_Obj* listdir(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(0, 1);
			const char* path = ".";
			if (argc == 1) {
				WG_EXPECT_ARG_TYPE_STRING(0);
				path = Wg_GetString(argv[0]);
			}

			std::error_code ec{};
			std::vector<fs::path> paths;
			for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
				paths.push_back(entry.path());
			}

			if (ec) {
				Wg_RaiseException(context, WG_EXC_OSERROR);
				return nullptr;
			}

			Wg_Obj* list = Wg_NewList(context);
			if (list == nullptr)
				return nullptr;
			Wg_ObjRef ref(list);

			for (const auto& path : paths) {
				Wg_Obj* entry = Wg_NewString(context, path.string().c_str());
				if (entry == nullptr)
					return nullptr;
				if (Wg_CallMethod(list, "append", &entry, 1) == nullptr)
					return nullptr;
			}

			return list;
		}

		static Wg_Obj* abort(Wg_Context* context, Wg_Obj**, int argc) {
			WG_EXPECT_ARG_COUNT(0);
			std::abort();
		}

		static Wg_Obj* chdir(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_STRING(0);
			const char* path = Wg_GetString(argv[0]);

			std::error_code ec{};
			fs::current_path(path, ec);
			if (ec) {
				Wg_RaiseException(context, WG_EXC_OSERROR);
				return nullptr;
			}

			return Wg_None(context);
		}

		static Wg_Obj* getcwd(Wg_Context* context, Wg_Obj**, int argc) {
			WG_EXPECT_ARG_COUNT(0);
			auto path = fs::current_path().string();
			return Wg_NewString(context, path.c_str());
		}
	}

	bool ImportOS(Wg_Context* context) {
		using namespace osmodule;
		try {
			RegisterFunction(context, "system", system);
			RegisterFunction(context, "chdir", chdir);
			RegisterFunction(context, "getcwd", getcwd);
			RegisterFunction(context, "mkdir", mkdir);
			RegisterFunction(context, "makedirs", makedirs);
			RegisterFunction(context, "remove", remove);
			RegisterFunction(context, "rmdir", rmdir);
			RegisterFunction(context, "rename", rename);
			RegisterFunction(context, "listdir", listdir);
			RegisterFunction(context, "abort", abort);
			
			Wg_SetGlobal(context, "error", Wg_GetGlobal(context, "OSError"));

#ifdef _WIN32
			RegisterConstant(context, "sep", Wg_NewString, "\\");
			RegisterConstant(context, "linesep", Wg_NewString, "\r\n");
#else
			RegisterConstant(context, "sep", Wg_NewString, "/");
			RegisterConstant(context, "linesep", Wg_NewString, "\r");
#endif
			return true;
		} catch (LibraryInitException&) {
			return false;
		}
	}
}


#include <unordered_map>
#include <functional>
#include <algorithm>
#include <iterator>

namespace wings {

	static thread_local std::vector<Statement::Type> statementHierarchy;

	static CodeError ParseBody(const LexTree& node, Statement::Type statType, std::vector<Statement>& out);

	static CodeError CheckTrailingTokens(const TokenIter& p) {
		if (!p.EndReached()) {
			return CodeError::Bad("Unexpected trailing tokens", p->srcPos);
		} else {
			return CodeError::Good();
		}
	}

	static CodeError ExpectColonEnding(TokenIter& p) {
		if (p.EndReached()) {
			return CodeError::Bad("Expected a ':'", (--p)->srcPos);
		} else if (p->text != ":") {
			return CodeError::Bad("Expected a ':'", p->srcPos);
		}
		++p;

		return CheckTrailingTokens(p);
	}

	static CodeError ParseConditionalBlock(const LexTree& node, Statement& out, Statement::Type type) {
		TokenIter p(node.tokens);
		++p;

		if (auto error = ParseExpression(p, out.expr)) {
			return error;
		}

		if (auto error = ExpectColonEnding(p)) {
			return error;
		}

		out.type = type;
		return ParseBody(node, type, out.body);
	}

	static CodeError ParseIf(const LexTree& node, Statement& out) {
		return ParseConditionalBlock(node, out, Statement::Type::If);
	}

	static CodeError ParseElif(const LexTree& node, Statement& out) {
		return ParseConditionalBlock(node, out, Statement::Type::Elif);
	}

	static CodeError ParseElse(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		++p;

		out.type = Statement::Type::Else;
		if (auto error = ExpectColonEnding(p)) {
			return error;
		}

		return ParseBody(node, Statement::Type::Else, out.body);
	}

	static CodeError ParseWhile(const LexTree& node, Statement& out) {
		return ParseConditionalBlock(node, out, Statement::Type::While);
	}

	static CodeError ParseVariableList(TokenIter& p, std::vector<std::string>& out) {
		out.clear();
		if (p.EndReached()) {
			return CodeError::Bad("Expected a variable name", (--p)->srcPos);
		} else if (p->type != Token::Type::Word) {
			return CodeError::Bad("Expected a variable name", p->srcPos);
		}

		while (true) {
			out.push_back(p->text);
			++p;

			if (p.EndReached()) {
				return CodeError::Good();
			} else if (p->text != ",") {
				return CodeError::Good();
			} 
			++p;

			if (p.EndReached()) {
				return CodeError::Good();
			} else if (p->type != Token::Type::Word) {
				return CodeError::Good();
			}
		}
	}

	Statement TransformForToWhile(Statement forLoop) {
		// __VarXXX = expression.__iter__()
		std::string rangeVarName = "__For" + std::to_string(Guid());

		Expression loadIter{};
		loadIter.srcPos = forLoop.expr.srcPos;
		loadIter.operation = Operation::Dot;
		loadIter.variableName = "__iter__";
		loadIter.children.push_back(std::move(forLoop.expr));

		Expression callIter{};
		callIter.srcPos = forLoop.expr.srcPos;
		callIter.operation = Operation::Call;
		callIter.children.push_back(std::move(loadIter));
		
		Statement rangeEval{};
		rangeEval.srcPos = forLoop.expr.srcPos;
		rangeEval.type = Statement::Type::Expr;
		rangeEval.expr.operation = Operation::Assign;
		rangeEval.expr.srcPos = forLoop.expr.srcPos;
		rangeEval.expr.assignTarget.type = AssignType::Direct;
		rangeEval.expr.assignTarget.direct = rangeVarName;
		rangeEval.expr.children.push_back({}); // Dummy
		rangeEval.expr.children.push_back(std::move(callIter));

		// while True:
		Expression condition{};
		condition.srcPos = forLoop.expr.srcPos;
		condition.operation = Operation::Literal;
		condition.literalValue.type = LiteralValue::Type::Bool;
		condition.literalValue.b = true;

		Statement wh{};
		wh.srcPos = forLoop.expr.srcPos;
		wh.type = Statement::Type::While;
		wh.expr = std::move(condition);

		// try:
		//		__VarXXX = __VarXXX.__next__()
		// except StopIteration:
		//		break
		Statement brk{};
		brk.srcPos = forLoop.expr.srcPos;
		brk.type = Statement::Type::Break;

		Expression stopIter{};
		stopIter.srcPos = forLoop.expr.srcPos;
		stopIter.operation = Operation::Variable;
		stopIter.variableName = "StopIteration";

		Statement except{};
		except.srcPos = forLoop.expr.srcPos;
		except.type = Statement::Type::Except;
		except.exceptBlock.exceptType = std::move(stopIter);
		except.body.push_back(std::move(brk));

		Statement tryExcept{};
		tryExcept.srcPos = forLoop.expr.srcPos;
		tryExcept.type = Statement::Type::Try;
		tryExcept.tryBlock.exceptClauses.push_back(std::move(except));

		// vars = __VarXXX.__next__()
		Expression rangeVar{};
		rangeVar.srcPos = forLoop.expr.srcPos;
		rangeVar.operation = Operation::Variable;
		rangeVar.variableName = rangeVarName;

		Expression loadNext{};
		loadNext.srcPos = forLoop.expr.srcPos;
		loadNext.operation = Operation::Dot;
		loadNext.variableName = "__next__";
		loadNext.children.push_back(std::move(rangeVar));

		Expression callNext{};
		callNext.srcPos = forLoop.expr.srcPos;
		callNext.operation = Operation::Call;
		callNext.children.push_back(std::move(loadNext));

		Expression iterAssign{};
		iterAssign.srcPos = forLoop.expr.srcPos;
		iterAssign.operation = Operation::Assign;
		iterAssign.assignTarget = forLoop.forLoop.assignTarget;
		iterAssign.children.push_back({}); // Dummy
		iterAssign.children.push_back(std::move(callNext));

		Statement iterAssignStat{};
		iterAssignStat.srcPos = forLoop.expr.srcPos;
		iterAssignStat.type = Statement::Type::Expr;
		iterAssignStat.expr = std::move(iterAssign);
		tryExcept.body.push_back(std::move(iterAssignStat));

		// Transfer body over
		wh.body.push_back(std::move(tryExcept));
		for (auto& child : forLoop.body)
			wh.body.push_back(std::move(child));

		Statement out{};
		out.srcPos = forLoop.expr.srcPos;
		out.type = Statement::Type::Composite;
		out.body.push_back(std::move(rangeEval));
		out.body.push_back(std::move(wh));
		return out;
	}

	CodeError ParseForLoopVariableList(TokenIter& p, std::vector<std::string>& vars, bool& isTuple) {
		bool mustTerminate = false;
		isTuple = false;
		while (true) {
			if (p.EndReached()) {
				return CodeError::Bad("Expected 'in'", (--p)->srcPos);
			} else if (p->text == "in") {
				if (vars.empty()) {
					return CodeError::Bad("Expected a variable name", p->srcPos);
				} else {
					return CodeError::Good();
				}
			} else if (mustTerminate) {
				return CodeError::Bad("Expected 'in'", p->srcPos);
			} else if (p->type != Token::Type::Word) {
				return CodeError::Bad("Expected a variable name", p->srcPos);
			}
			vars.push_back(p->text);
			++p;

			if (!p.EndReached() && p->text == ",") {
				isTuple = true;
				++p;
			} else {
				mustTerminate = true;
			}
		}
	}

	static CodeError ParseFor(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		++p;
		out.type = Statement::Type::For;

		std::vector<std::string> vars;
		bool isTuple{};
		if (auto error = ParseForLoopVariableList(p, vars, isTuple)) {
			return error;
		}
		++p;

		if (!isTuple) {
			out.forLoop.assignTarget.type = AssignType::Direct;
			out.forLoop.assignTarget.direct = vars[0];
		} else {
			out.forLoop.assignTarget.type = AssignType::Pack;
			for (auto& var : vars) {
				AssignTarget elem{};
				elem.type = AssignType::Direct;
				elem.direct = std::move(var);
				out.forLoop.assignTarget.pack.push_back(std::move(elem));
			}
		}

		if (auto error = ParseExpression(p, out.expr)) {
			return error;
		}

		if (auto error = ExpectColonEnding(p)) {
			return error;
		}

		if (auto error = ParseBody(node, Statement::Type::For, out.body)) {
			return error;
		}

		out = TransformForToWhile(std::move(out));
		return CodeError::Good();
	}

	CodeError ParseParameterList(TokenIter& p, std::vector<Parameter>& out) {
		out.clear();
		Parameter::Type type = Parameter::Type::Named;
		while (true) {
			if (p.EndReached()) {
				return CodeError::Good();
			} else if (p->text == "*") {
				if (type == Parameter::Type::ListArgs) {
					return CodeError::Bad("Only 1 variadic arguments parameter is allowed", p->srcPos);
				} else if (type == Parameter::Type::Kwargs) {
					return CodeError::Bad("Keyword arguments parameter must appear last", p->srcPos);
				}
				type = Parameter::Type::ListArgs;
				++p;
			} else if (p->text == "**") {
				if (type == Parameter::Type::Kwargs) {
					return CodeError::Bad("Only 1 keyword arguments parameter is allowed", p->srcPos);
				}
				type = Parameter::Type::Kwargs;
				++p;
			} else if (p->type != Token::Type::Word) {
				return CodeError::Good();
			} else {
				if (type != Parameter::Type::Named) {
					return CodeError::Bad("Regular parameters must appear first", p->srcPos);
				}
			}

			if (p.EndReached()) {
				return CodeError::Bad("Expected a parameter name", (--p)->srcPos);
			} else if (p->type != Token::Type::Word) {
				return CodeError::Bad("Expected a parameter name", p->srcPos);
			}

			std::string parameterName = p->text;

			// Check for duplicate parameters
			if (std::find_if(out.begin(), out.end(), [&](const Parameter& p) {
				return p.name == parameterName;
				}) != out.end()) {
				return CodeError::Bad("Duplicate parameter name", p->srcPos);
			}
			++p;

			std::optional<Expression> defaultValue;
			if (p.EndReached()) {
				out.push_back(Parameter{ parameterName, std::nullopt, type });
				return CodeError::Good();
			} else if (p->text == "=") {
				// Default value
				if (type != Parameter::Type::Named) {
					return CodeError::Bad("Only regular parameters can have a default argument", p->srcPos);
				}
				++p;
				Expression expr{};
				if (auto error = ParseExpression(p, expr)) {
					return error;
				}
				defaultValue = std::move(expr);
			} else if (!out.empty() && out.back().defaultValue) {
				// If last parameter has a default value,
				// this parameter must also have a default value
				return CodeError::Bad(
					"Parameters with default values must appear at the end of the parameter list",
					(--p)->srcPos
				);
			}

			out.push_back(Parameter{ std::move(parameterName), std::move(defaultValue), type });

			if (p.EndReached()) {
				return CodeError::Good();
			} else if (p->text != ",") {
				return CodeError::Good();
			}
			++p;
		}
	}

	std::unordered_set<std::string> GetReferencedVariables(const AssignTarget& target) {
		if (target.type == AssignType::Direct) {
			return { target.direct };
		} else {
			std::unordered_set<std::string> variables;
			for (const auto& child : target.pack)
				variables.merge(GetReferencedVariables(child));
			return variables;
		}
	}

	// Get a set of variables referenced by an expression
	std::unordered_set<std::string> GetReferencedVariables(const Expression& expr) {
		std::unordered_set<std::string> variables;
		if (expr.operation == Operation::Variable) {
			variables.insert(expr.variableName);
		} else {
			for (const auto& child : expr.children) {
				variables.merge(GetReferencedVariables(child));
			}
		}
		return variables;
	}

	// Get a set of variables directly written to by the '=' operator. This excludes compound assignment.
	static std::unordered_set<std::string> GetWriteVariables(const Expression& expr) {
		if (expr.operation == Operation::Assign && (expr.assignTarget.type == AssignType::Direct || expr.assignTarget.type == AssignType::Pack)) {
			return GetReferencedVariables(expr.assignTarget);
		} else {
			std::unordered_set<std::string> variables;
			for (const auto& child : expr.children)
				variables.merge(GetWriteVariables(child));
			return variables;
		}
	}

	template <typename T, typename Subtract, typename... Args>
	static std::unordered_set<T> SetDifference(const std::unordered_set<T>& set, const Subtract& subtract, const Args&... args) {
		if constexpr (sizeof...(args) == 0) {
			std::unordered_set<T> diff = set;
			for (const auto& sub : subtract)
				diff.erase(sub);
			return diff;
		} else {
			return SetDifference(SetDifference(set, subtract), args...);
		}
	}

	static void ResolveCaptures(Statement& defNode) {
		std::unordered_set<std::string> writeVars;
		std::unordered_set<std::string> allVars;

		std::function<void(const std::vector<Statement>&)> scanNode = [&](const std::vector<Statement>& body) {
			for (const auto& child : body) {
				bool isFn = child.expr.operation == Operation::Function;
				switch (child.type) {
				case Statement::Type::Expr:
				case Statement::Type::If:
				case Statement::Type::Elif:
				case Statement::Type::While:
				case Statement::Type::Return:
					if (isFn) {
						writeVars.insert(child.expr.def.name);
						allVars.insert(child.expr.def.name);
						for (const auto& parameter : child.expr.def.parameters) {
							if (parameter.defaultValue) {
								writeVars.merge(GetWriteVariables(parameter.defaultValue.value()));
								allVars.merge(GetReferencedVariables(parameter.defaultValue.value()));
							}
						}
						allVars.insert(child.expr.def.localCaptures.begin(), child.expr.def.localCaptures.end());
					} else {
						writeVars.merge(GetWriteVariables(child.expr));
						allVars.merge(GetReferencedVariables(child.expr));
					}
					break;
				case Statement::Type::Class:
					writeVars.insert(child.klass.name);
					allVars.insert(child.klass.name);
					break;
				case Statement::Type::Def:
					writeVars.insert(child.expr.def.name);
					allVars.insert(child.expr.def.name);
					break;
				case Statement::Type::Global:
					defNode.expr.def.globalCaptures.insert(child.capture.name);
					break;
				case Statement::Type::Nonlocal:
					defNode.expr.def.localCaptures.insert(child.capture.name);
					break;
				}

				if (!isFn) {
					scanNode(child.body);
				}
			}
		};

		scanNode(defNode.expr.def.body);

		std::vector<std::string> parameterVars;
		for (const auto& param : defNode.expr.def.parameters)
			parameterVars.push_back(param.name);
		defNode.expr.def.localCaptures.merge(SetDifference(allVars, writeVars, parameterVars));
		defNode.expr.def.variables = SetDifference(writeVars, defNode.expr.def.globalCaptures, defNode.expr.def.localCaptures, parameterVars);
	}

	static CodeError ParseDef(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		out.type = Statement::Type::Def;
		++p;

		Expression fn{};
		fn.srcPos = node.tokens[0].srcPos;
		fn.operation = Operation::Function;

		if (p.EndReached()) {
			return CodeError::Bad("Expected a function name", (--p)->srcPos);
		} else if (p->type != Token::Type::Word) {
			return CodeError::Bad("Expected a function name", p->srcPos);
		}
		fn.def.name = p->text;
		++p;

		if (p.EndReached()) {
			return CodeError::Bad("Expected a '('", (--p)->srcPos);
		} else if (p->text != "(") {
			return CodeError::Bad("Expected a '('", p->srcPos);
		}
		++p;

		if (auto error = ParseParameterList(p, fn.def.parameters)) {
			return error;
		}

		if (p.EndReached()) {
			return CodeError::Bad("Expected a ')'", (--p)->srcPos);
		} else if (p->text != ")") {
			return CodeError::Bad("Expected a ')'", p->srcPos);
		}
		++p;

		if (auto error = ExpectColonEnding(p)) {
			return error;
		}

		if (auto error = ParseBody(node, Statement::Type::Def, fn.def.body)) {
			return error;
		}

		out.expr = std::move(fn);

		ResolveCaptures(out);

		return CodeError::Good();
	}

	static CodeError ParseClass(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		out.type = Statement::Type::Class;
		++p;

		if (p.EndReached()) {
			return CodeError::Bad("Expected a class name", (--p)->srcPos);
		} else if (p->type != Token::Type::Word) {
			return CodeError::Bad("Expected a class name", p->srcPos);
		}
		out.klass.name = p->text;
		++p;

		if (p.EndReached()) {
			return CodeError::Bad("Expected a ':'", (--p)->srcPos);
		} else if (p->text == "(") {
			++p;
			if (auto error = ParseExpressionList(p, ")", out.klass.bases)) {
				return error;
			}
			++p;
		}

		if (node.children.empty()) {
			return CodeError::Bad("Expected class body", (--p)->srcPos);
		}

		if (auto error = ExpectColonEnding(p)) {
			return error;
		}

		for (const auto& method : node.children) {
			if (method.tokens[0].text == "pass") {
				continue;
			} else if (method.tokens[0].text != "def") {
				return CodeError::Bad("Expected a method definition");
			}

			Statement stat{};
			if (auto error = ParseDef(method, stat)) {
				return error;
			}
			stat.srcPos = method.tokens[0].srcPos;
			out.klass.methodNames.push_back(stat.expr.def.name);
			out.body.push_back(std::move(stat));
		}

		return CodeError::Good();
	}

	static CodeError ParseTry(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		++p;

		if (auto error = ExpectColonEnding(p)) {
			return error;
		}

		out.type = Statement::Type::Try;
		return ParseBody(node, Statement::Type::Try, out.body);
	}

	static CodeError ParseExcept(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		++p;

		Expression exceptType{};
		if (p.EndReached()) {
			return CodeError::Bad("Expected a ':'", (--p)->srcPos);
		} else if (p->text == ":") {
			goto end;
		} else if (auto error = ParseExpression(p, exceptType)) {
			return error;
		}
		out.exceptBlock.exceptType = std::move(exceptType);

		if (p.EndReached()) {
			return CodeError::Bad("Expected a ':'", (--p)->srcPos);
		} else if (p->text == ":") {
			goto end;
		} else if (p->text != "as") {
			return CodeError::Bad("Expected a 'as'", p->srcPos);
		}
		++p;

		if (p.EndReached()) {
			return CodeError::Bad("Expected an identifier", (--p)->srcPos);
		} else if (p->type != Token::Type::Word) {
			return CodeError::Bad("Expected an identifier", p->srcPos);
		}
		out.exceptBlock.var = p->text;
		++p;
		
	end:
		if (auto error = ExpectColonEnding(p)) {
			return error;
		}

		out.type = Statement::Type::Except;
		return ParseBody(node, Statement::Type::Except, out.body);
	}

	static CodeError ParseFinally(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		++p;

		if (auto error = ExpectColonEnding(p)) {
			return error;
		}

		out.type = Statement::Type::Finally;
		return ParseBody(node, Statement::Type::Finally, out.body);
	}

	static CodeError ParseRaise(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		++p;

		out.type = Statement::Type::Raise;
		if (auto error = ParseExpression(p, out.expr)) {
			return error;
		} else {
			return CheckTrailingTokens(p);
		}
	}

	static CodeError ParseWith(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		SourcePosition srcPos = p->srcPos;
		++p;

		Expression manager{};
		if (p.EndReached()) {
			return CodeError::Bad("Expected a ':'", (--p)->srcPos);
		} else if (auto error = ParseExpression(p, manager)) {
			return error;
		}

		std::string var;
		if (p.EndReached()) {
			return CodeError::Bad("Expected a ':'", (--p)->srcPos);
		} else if (p->text == ":") {
			goto end;
		} else if (p->text != "as") {
			return CodeError::Bad("Expected a 'as'", p->srcPos);
		}
		++p;

		if (p.EndReached()) {
			return CodeError::Bad("Expected an identifier", (--p)->srcPos);
		} else if (p->type != Token::Type::Word) {
			return CodeError::Bad("Expected an identifier", p->srcPos);
		}
		var = p->text;
		++p;

	end:
		if (auto error = ExpectColonEnding(p)) {
			return error;
		}

		std::vector<Statement> body;
		if (auto error = ParseBody(node, Statement::Type::Composite, body)) {
			return error;
		}

		/*
		 * __WithMgr = <expr>
		 * [<var> =] __WithMgr.__enter__()
		 * try:
		 *		<body>
		 * finally:
		 * 		__WithMgr.__exit__(None, None, None)
		 */

		std::vector<Statement> mainBody;

		// __WithMgr = <expr>
		std::string mgrName = "__WithMgr" + std::to_string(Guid());
		Expression assignMgr{};
		assignMgr.srcPos = srcPos;
		assignMgr.operation = Operation::Assign;
		assignMgr.assignTarget.type = AssignType::Direct;
		assignMgr.assignTarget.direct = mgrName;
		assignMgr.children.push_back({}); // Dummy
		assignMgr.children.push_back(std::move(manager));

		Statement assignMgrStat{};
		assignMgrStat.srcPos = srcPos;
		assignMgrStat.type = Statement::Type::Expr;
		assignMgrStat.expr = std::move(assignMgr);
		mainBody.push_back(std::move(assignMgrStat));

		// [<var> =] __WithMgr.__enter__()
		auto loadMgr = [&] {
			Expression load{};
			load.srcPos = srcPos;
			load.operation = Operation::Variable;
			load.variableName = mgrName;
			return load;
		};

		Expression enter{};
		enter.srcPos = srcPos;
		enter.operation = Operation::Dot;
		enter.variableName = "__enter__";
		enter.children.push_back(loadMgr());

		Expression enterCall{};
		enterCall.srcPos = srcPos;
		enterCall.operation = Operation::Call;
		enterCall.children.push_back(std::move(enter));

		Statement enterStat{};
		enterStat.srcPos = srcPos;
		enterStat.type = Statement::Type::Expr;
		if (!var.empty()) {
			Expression assign{};
			assign.srcPos = srcPos;
			assign.operation = Operation::Assign;
			assign.assignTarget.type = AssignType::Direct;
			assign.assignTarget.direct = std::move(var);
			assign.children.push_back({}); // Dummy
			assign.children.push_back(std::move(enterCall));
			enterStat.expr = std::move(assign);
		} else {
			enterStat.expr = std::move(enterCall);
		}
		mainBody.push_back(std::move(enterStat));

		// __WithMgr.__exit__(None, None, None)
		Expression loadExit{};
		loadExit.srcPos = srcPos;
		loadExit.operation = Operation::Dot;
		loadExit.variableName = "__exit__";
		loadExit.children.push_back(loadMgr());

		auto loadNone = [&] {
			Expression none{};
			none.srcPos = srcPos;
			none.operation = Operation::Literal;
			none.literalValue.type = LiteralValue::Type::Null;
			return none;
		};
		
		Expression exit{};
		exit.srcPos = srcPos;
		exit.operation = Operation::Call;
		exit.children.push_back(std::move(loadExit));
		exit.children.push_back(loadNone());
		exit.children.push_back(loadNone());
		exit.children.push_back(loadNone());

		Statement exitStat{};
		exitStat.srcPos = srcPos;
		exitStat.type = Statement::Type::Expr;
		exitStat.expr = std::move(exit);

		// try/finally
		Statement tryBlock{};
		tryBlock.srcPos = srcPos;
		tryBlock.type = Statement::Type::Try;
		tryBlock.body = std::move(body);
		tryBlock.tryBlock.finallyClause.push_back(std::move(exitStat));
		mainBody.push_back(std::move(tryBlock));

		// Produce composite statement
		out.type = Statement::Type::Composite;
		out.body = std::move(mainBody);
		return CodeError::Good();
	}

	static CodeError ParseReturn(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		++p;

		out.type = Statement::Type::Return;
		if (p.EndReached()) {
			out.expr.operation = Operation::Literal;
			out.expr.literalValue.type = LiteralValue::Type::Null;
			return CodeError::Good();
		} else if (auto error = ParseExpression(p, out.expr)) {
			return error;
		} else {
			return CheckTrailingTokens(p);
		}
	}

	static CodeError ParseSingleToken(const LexTree& node, Statement& out, Statement::Type type) {
		TokenIter p(node.tokens);
		++p;
		out.type = type;
		return CheckTrailingTokens(p);
	}

	static CodeError CheckBreakable(const LexTree& node) {
		auto it = statementHierarchy.rbegin();
		while (true) {
			if (*it == Statement::Type::Def || *it == Statement::Type::Root) {
				return CodeError::Bad("'break' or 'continue' outside of loop", node.tokens[0].srcPos);
			} else if (*it == Statement::Type::For || *it == Statement::Type::While) {
				return CodeError::Good();
			}
			++it;
		}
	}

	static CodeError ParseBreak(const LexTree& node, Statement& out) {
		if (auto error = CheckBreakable(node)) {
			return error;
		}
		return ParseSingleToken(node, out, Statement::Type::Break);
	}

	static CodeError ParseContinue(const LexTree& node, Statement& out) {
		if (auto error = CheckBreakable(node)) {
			return error;
		}
		return ParseSingleToken(node, out, Statement::Type::Continue);
	}

	static CodeError ParsePass(const LexTree& node, Statement& out) {
		return ParseSingleToken(node, out, Statement::Type::Pass);
	}

	static CodeError ParseCapture(const LexTree& node, Statement& out, Statement::Type type) {
		TokenIter p(node.tokens);
		++p;

		if (statementHierarchy.back() == Statement::Type::Root) {
			return CodeError::Bad("Cannot capture at top level", (--p)->srcPos);
		}

		if (p.EndReached()) {
			return CodeError::Bad("Expected a variable name", (--p)->srcPos);
		} else if (p->type != Token::Type::Word) {
			return CodeError::Bad("Expected a variable name", p->srcPos);
		}

		out.type = type;
		out.capture.name = p->text;
		++p;
		return CheckTrailingTokens(p);
	}

	static CodeError ParseNonlocal(const LexTree& node, Statement& out) {
		return ParseCapture(node, out, Statement::Type::Nonlocal);
	}

	static CodeError ParseGlobal(const LexTree& node, Statement& out) {
		return ParseCapture(node, out, Statement::Type::Global);
	}

	static CodeError ParseExpressionStatement(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		out.type = Statement::Type::Expr;
		if (auto error = ParseExpression(p, out.expr)) {
			return error;
		} else {
			return CheckTrailingTokens(p);
		}
	}

	static CodeError ParseImportFrom(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		out.type = Statement::Type::ImportFrom;
		++p;

		if (p.EndReached()) {
			return CodeError::Bad("Expected a module name", (--p)->srcPos);
		} else if (p->type != Token::Type::Word) {
			return CodeError::Bad("Expected a module name", p->srcPos);
		}

		out.importFrom.module = p->text;
		++p;

		if (p.EndReached()) {
			return CodeError::Bad("Expected 'import'", (--p)->srcPos);
		} else if (p->text != "import") {
			return CodeError::Bad("Expected 'import'", p->srcPos);
		}
		++p;

		if (p.EndReached()) {
			return CodeError::Bad("Expected a name", (--p)->srcPos);
		}
		
		if (p->text == "*") {
			++p;
		} else {
			while (true) {
				if (p->type != Token::Type::Word) {
					return CodeError::Bad("Expected a name", p->srcPos);
				}
				out.importFrom.names.push_back(p->text);
				++p;

				if (p.EndReached()) {
					break;
				}

				if (p->text == "as") {
					++p;
					if (p.EndReached()) {
						return CodeError::Bad("Expected a name", (--p)->srcPos);
					} else if (p->type != Token::Type::Word) {
						return CodeError::Bad("Expected a name", p->srcPos);
					}
					out.importFrom.alias = p->text;
					++p;
					break;
				}
				
				if (p->text == ",") {
					++p;
				} else {
					return CodeError::Bad("Expected ','", p->srcPos);
				}
			}
		}

		return CheckTrailingTokens(p);
	}

	static CodeError ParseImport(const LexTree& node, Statement& out) {
		TokenIter p(node.tokens);
		out.type = Statement::Type::Import;
		++p;
		
		if (p.EndReached()) {
			return CodeError::Bad("Expected a module name", (--p)->srcPos);
		} else if (p->type != Token::Type::Word) {
			return CodeError::Bad("Expected a module name", p->srcPos);
		}

		out.import.module = p->text;
		++p;

		if (!p.EndReached() && p->text == "as") {
			++p;
			if (p.EndReached()) {
				return CodeError::Bad("Expected an alias name", (--p)->srcPos);
			} else if (p->type != Token::Type::Word) {
				return CodeError::Bad("Expected an alias name", p->srcPos);
			}
			out.import.alias = p->text;
			++p;
		}
		
		return CheckTrailingTokens(p);
	}

	using ParseFn = CodeError(*)(const LexTree& node, Statement& out);

	static const std::unordered_map<std::string, ParseFn> STATEMENT_STARTINGS = {
		{ "if", ParseIf },
		{ "elif", ParseElif },
		{ "else", ParseElse },
		{ "while", ParseWhile },
		{ "for", ParseFor },
		{ "break", ParseBreak },
		{ "continue", ParseContinue },
		{ "def", ParseDef },
		{ "class", ParseClass },
		{ "return", ParseReturn },
		{ "pass", ParsePass },
		{ "nonlocal", ParseNonlocal },
		{ "global", ParseGlobal },
		{ "try", ParseTry },
		{ "except", ParseExcept },
		{ "finally", ParseFinally },
		{ "raise", ParseRaise },
		{ "with", ParseWith },
		{ "from", ParseImportFrom },
		{ "import", ParseImport },
	};

	static CodeError ParseStatement(const LexTree& node, Statement& out) {
		const auto& firstToken = node.tokens[0].text;
		if (STATEMENT_STARTINGS.contains(firstToken)) {
			if (auto error = STATEMENT_STARTINGS.at(firstToken)(node, out)) {
				return error;
			}
		} else {
			if (auto error = ParseExpressionStatement(node, out)) {
				return error;
			}
		}

		out.srcPos = node.tokens[0].srcPos;
		return CodeError::Good();
	}

	void ExpandCompositeStatements(std::vector<Statement>& statements) {
		for (size_t i = 0; i < statements.size(); i++) {
			if (statements[i].type == Statement::Type::Composite) {
				for (size_t j = 0; j < statements[i].body.size(); j++) {
					auto& child = statements[i].body[j];
					statements.insert(statements.begin() + i + j + 1, std::move(child));
				}
				statements.erase(statements.begin() + i);
			}
		}
	}

	static CodeError ParseBody(const LexTree& node, Statement::Type statType, std::vector<Statement>& out) {
		out.clear();

		if (node.children.empty()) {
			return CodeError::Bad("Expected a statement", node.tokens.back().srcPos);
		}

		statementHierarchy.push_back(statType);
		for (auto& node : node.children) {
			Statement statement;
			if (auto error = ParseStatement(node, statement)) {
				out.clear();
				return error;
			}
			out.push_back(std::move(statement));
		}
		statementHierarchy.pop_back();

		ExpandCompositeStatements(out);

		// Validate elif and else
		for (size_t i = 0; i < out.size(); i++) {
			auto& stat = out[i];
			Statement::Type lastType = i ? out[i - 1].type : Statement::Type::Pass;

			if (stat.type == Statement::Type::Elif) {
				if (lastType != Statement::Type::If && lastType != Statement::Type::Elif) {
					return CodeError::Bad(
						"An 'elif' clause may only appear after an 'if' or 'elif' clause",
						stat.srcPos
					);
				}
			} else if (stat.type == Statement::Type::Else) {
				if (lastType != Statement::Type::If && lastType != Statement::Type::Elif && lastType != Statement::Type::While) {
					return CodeError::Bad(
						"An 'else' clause may only appear after an 'if', 'elif', 'while', or 'for' clause",
						stat.srcPos
					);
				}
			}
		}

		// Rearrange elif and else nodes
		for (size_t i = 0; i < out.size(); i++) {
			auto& stat = out[i];

			std::optional<Statement> elseClause;
			if (stat.type == Statement::Type::Elif) {
				// Transform elif into an else and if statement
				stat.type = Statement::Type::If;
				elseClause = Statement{};
				elseClause.value().srcPos = stat.srcPos;
				elseClause.value().type = Statement::Type::Else;
				elseClause.value().body.push_back(std::move(stat));
				out.erase(out.begin() + i);
				i--;

			} else if (stat.type == Statement::Type::Else) {
				elseClause = std::move(stat);
				out.erase(out.begin() + i);
				i--;
			}

			if (elseClause) {
				Statement* parent = &out[i];
				while (parent->elseClause) {
					parent = &parent->elseClause->body.back();
				}
				parent->elseClause = std::make_unique<Statement>(std::move(elseClause.value()));
			}
		}

		for (size_t i = 0; i < out.size(); i++) {
			SourcePosition srcPos = out[i].srcPos;
			switch (out[i].type) {
			case Statement::Type::Except:
				return CodeError::Bad("An 'except' clause may only appear after a 'try' or 'except' clause", srcPos);
			case Statement::Type::Finally:
				return CodeError::Bad("A 'finally' clause may only appear after a 'try' or 'except' clause", srcPos);
			case Statement::Type::Try: {
				auto& tryStat = out[i];
				
				for (i++; i < out.size(); i++) {
					srcPos = out[i].srcPos;
					switch (out[i].type) {
					case Statement::Type::Except: {
						auto& exceptClauses = tryStat.tryBlock.exceptClauses;
						if (!exceptClauses.empty() && !exceptClauses.back().exceptBlock.exceptType.has_value()) {
							return CodeError::Bad("Default 'except' clause must be last", srcPos);
						}
						exceptClauses.push_back(std::move(out[i]));
						out.erase(out.begin() + i);
						i--;
						break;
					}
					case Statement::Type::Finally:
						tryStat.tryBlock.finallyClause = std::move(out[i].body);
						out.erase(out.begin() + i);
						i--;
						goto end;
					default:
						goto end;
					}
				}

			end:
				if (tryStat.tryBlock.exceptClauses.empty() && tryStat.tryBlock.finallyClause.empty()) {
					return CodeError::Bad("Expected an 'except' or 'finally' clause", srcPos);
				}
				i--;
			}
			}
		}

		return CodeError::Good();
	}

	ParseResult Parse(const LexTree& lexTree) {
		ParseResult result{};
		result.parseTree.type = Statement::Type::Root;

		if (lexTree.children.empty()) {
			return result;
		}

		statementHierarchy.clear();
		result.error = ParseBody(lexTree, Statement::Type::Root, result.parseTree.expr.def.body);
		statementHierarchy.clear();

		ResolveCaptures(result.parseTree);
		result.parseTree.expr.def.variables.merge(result.parseTree.expr.def.localCaptures);
		result.parseTree.expr.def.localCaptures.clear();

		return result;
	}
}


namespace wings {
	bool ImportRandom(Wg_Context* context);
}


#include <random>

namespace wings {
	namespace randommodule {
		static constexpr const char* RAND_CODE = R"(
def choice(seq):
	t = tuple(seq)
	return t[randint(0, len(t) - 1)]

def getrandbits(n):
	x = 0
	for i in range(n):
		x <<= 1
		if random() < 0.5:
			x |= 1
	return x

def randrange(*args):
	return choice(range(*args))
		)";

		static Wg_Obj* randint(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(0);
			WG_EXPECT_ARG_TYPE_INT(1);
			Wg_int lower = Wg_GetInt(argv[0]);
			Wg_int upper = Wg_GetInt(argv[1]);
			return Wg_NewInt(context, context->rng.Int(lower, upper));
		}

		static Wg_Obj* random(Wg_Context* context, Wg_Obj**, int argc) {
			WG_EXPECT_ARG_COUNT(0);
			return Wg_NewFloat(context, context->rng.Rand());
		}

		static Wg_Obj* seed(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_INT(0);
			context->rng.Seed(Wg_GetInt(argv[0]));
			return Wg_None(context);
		}

		static Wg_Obj* shuffle(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_LIST(0);
			auto& li = argv[0]->Get<std::vector<Wg_Obj*>>();
			std::shuffle(li.begin(), li.end(), context->rng.Engine());
			return Wg_None(context);
		}

		static Wg_Obj* uniform(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			Wg_float lower = Wg_GetFloat(argv[0]);
			Wg_float upper = Wg_GetFloat(argv[1]);
			if (lower > upper) {
				Wg_RaiseException(context, WG_EXC_VALUEERROR, "Lower bound must be less than or equal to upper bound");
				return nullptr;
			}
			return Wg_NewFloat(context, context->rng.Float(lower, upper));
		}
	}

	bool ImportRandom(Wg_Context* context) {
		using namespace randommodule;
		try {
			RegisterFunction(context, "seed", seed);
			RegisterFunction(context, "shuffle", shuffle);
			RegisterFunction(context, "randint", randint);
			RegisterFunction(context, "random", random);
			RegisterFunction(context, "uniform", uniform);

			if (!Execute(context, RAND_CODE, "random"))
				throw LibraryInitException();
			
			return true;
		} catch (LibraryInitException&) {
			return false;
		}
	}
}


namespace wings {
	bool ImportSys(Wg_Context* context);
}


namespace wings {
	namespace sysmodule {
		static Wg_Obj* exit(Wg_Context* context, Wg_Obj**, int) {
			Wg_RaiseException(context, WG_EXC_SYSTEMEXIT);
			return nullptr;
		}
	}
	
	bool ImportSys(Wg_Context* context) {
		using namespace sysmodule;
		try {
			RegisterFunction(context, "exit", exit);
			Wg_SetGlobal(context, "argv", context->argv);
			return true;
		} catch (LibraryInitException&) {
			return false;
		}
	}
}


namespace wings {
	bool ImportTime(Wg_Context* context);
}


#include <chrono>
#include <thread>

namespace wings {
	namespace timemodule {
		static Wg_Obj* sleep(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			Wg_float secs = Wg_GetFloat(argv[0]);
			int ms = (int)(secs * 1000);
			std::this_thread::sleep_for(std::chrono::milliseconds(ms));
			return Wg_None(context);
		}

		static Wg_Obj* time(Wg_Context* context, Wg_Obj**, int argc) {
			WG_EXPECT_ARG_COUNT(0);
			auto now = std::chrono::system_clock::now();
			auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
			auto value = now_ms.time_since_epoch();
			return Wg_NewFloat(context, (Wg_float)value.count() / 1000);
		}
	}
	
	bool ImportTime(Wg_Context* context) {
		using namespace timemodule;
		try {
			RegisterFunction(context, "time", time);
			RegisterFunction(context, "sleep", sleep);
			
			return true;
		} catch (LibraryInitException&) {
			return false;
		}
	}
}



#include <iostream>
#include <memory>
#include <fstream>
#include <vector>
#include <string>
#include <string_view>
#include <sstream>
#include <queue>
#include <unordered_set>
#include <cstring>

namespace wings {
	static bool ReadFromFile(const std::string& path, std::string& data) {
		std::ifstream f(path);
		if (!f.is_open())
			return false;

		std::stringstream buffer;
		buffer << f.rdbuf();

		if (!f)
			return false;

		data = buffer.str();
		return true;
	}

	static bool LoadFileModule(Wg_Context* context, const std::string& module) {
		std::string path = context->importPath + module + ".py";
		std::string source;
		if (!ReadFromFile(path, source)) {
			std::string msg = std::string("No module named '") + module + "'";
			Wg_RaiseException(context, WG_EXC_IMPORTERROR, msg.c_str());
			return false;
		}

		Wg_Obj* fn = Compile(context, source.c_str(), module.c_str(), module.c_str(), false);
		if (fn == nullptr)
			return false;

		return Wg_Call(fn, nullptr, 0) != nullptr;
	}

	static bool LoadModule(Wg_Context* context, const std::string& name) {
		if (!context->globals.contains(name)) {
			bool success{};
			context->globals.insert({ std::string(name), {} });
			context->currentModule.push(name);

			if (name != "__builtins__")
				Wg_ImportAllFromModule(context, "__builtins__");

			auto it = context->moduleLoaders.find(name);
			if (it != context->moduleLoaders.end()) {
				success = it->second(context);
			} else {
				success = LoadFileModule(context, name);
			}

			context->currentModule.pop();
			if (!success) {
				context->globals.erase(name);
				return false;
			}
		}
		return true;
	}
}

extern "C" {
	void Wg_DefaultConfig(Wg_Config* config) {
		WG_ASSERT_VOID(config);
		config->maxAlloc = 1'000'000;
		config->maxRecursion = 50;
		config->gcRunFactor = 2.0f;
		config->printUserdata = nullptr;
		config->argv = nullptr;
		config->argc = 0;
		config->enableOSAccess = false;
		config->print = [](const char* message, int len, void*) {
			std::cout << std::string_view(message, (size_t)len);
		};
	}

	Wg_Context* Wg_CreateContext(const Wg_Config* config) {
		Wg_Context* context = new Wg_Context();
		
		context->currentModule.push("__main__");
		context->globals.insert({ std::string("__main__"), {} });

		// Initialise the library without restriction
		Wg_DefaultConfig(&context->config);
		
		Wg_RegisterModule(context, "__builtins__", wings::ImportBuiltins);
		Wg_RegisterModule(context, "dis", wings::ImportDis);
		Wg_RegisterModule(context, "math", wings::ImportMath);
		Wg_RegisterModule(context, "random", wings::ImportRandom);
		Wg_RegisterModule(context, "sys", wings::ImportSys);
		Wg_RegisterModule(context, "time", wings::ImportTime);
		Wg_ImportAllFromModule(context, "__builtins__");
		
		if (config) {
			WG_ASSERT(context);
			WG_ASSERT(config->maxAlloc >= 0);
			WG_ASSERT(config->maxRecursion >= 0);
			WG_ASSERT(config->gcRunFactor >= 1.0f);
			WG_ASSERT(config->argc >= 0);
			if (config->argc) {
				WG_ASSERT(config->argv);
				for (int i = 0; i < config->argc; i++)
					WG_ASSERT(config->argv[i]);
			}

			if (config->importPath) {
				context->importPath = config->importPath;
				if (config->importPath[0] != '/' && config->importPath[0] != '\\')
					context->importPath += "/";
			}

			context->config = *config;
		}
		
		if (context->config.enableOSAccess) {
			Wg_RegisterModule(context, "os", wings::ImportOS);
		}
		
		if (!wings::InitArgv(context, context->config.argv, context->config.argc))
			return nullptr;
		
		return context;
	}

	void Wg_DestroyContext(Wg_Context* context) {
		WG_ASSERT_VOID(context);
		context->closing = true;
		Wg_CollectGarbage(context);
		delete context;
	}

	void Wg_Print(const Wg_Context* context, const char* message, int len) {
		WG_ASSERT_VOID(context && message);
		if (context->config.print) {
			context->config.print(len ? message : "", len, context->config.printUserdata);
		}
	}

	void Wg_PrintString(const Wg_Context* context, const char* message) {
		WG_ASSERT_VOID(context && message);
		Wg_Print(context, message, (int)std::strlen(message));
	}

	void Wg_SetErrorCallback(Wg_ErrorCallback callback) {
		wings::errorCallback = callback;
	}

	Wg_Obj* Wg_Compile(Wg_Context* context, const char* script, const char* prettyName) {
		return wings::Compile(context, script, "__main__", prettyName, false);
	}

	Wg_Obj* Wg_CompileExpression(Wg_Context* context, const char* script, const char* prettyName) {
		return wings::Compile(context, script, "__main__", prettyName, true);
	}

	bool Wg_Execute(Wg_Context* context, const char* script, const char* prettyName) {
		if (Wg_Obj* fn = Wg_Compile(context, script, prettyName)) {
			return Wg_Call(fn, nullptr, 0) != nullptr;
		}
		return false;
	}
	
	Wg_Obj* Wg_ExecuteExpression(Wg_Context* context, const char* script, const char* prettyName) {
		if (Wg_Obj* fn = Wg_CompileExpression(context, script, prettyName)) {
			return Wg_Call(fn, nullptr, 0);
		}
		return nullptr;
	}

	Wg_Obj* Wg_GetGlobal(Wg_Context* context, const char* name) {
		WG_ASSERT(context && name && wings::IsValidIdentifier(name));
		auto module = std::string(context->currentModule.top());
		auto& globals = context->globals.at(module);
		auto it = globals.find(name);
		if (it == globals.end()) {
			return nullptr;
		} else {
			return *it->second;
		}
	}

	void Wg_SetGlobal(Wg_Context* context, const char* name, Wg_Obj* value) {
		WG_ASSERT_VOID(context && name && value && wings::IsValidIdentifier(name));
		const auto& module = std::string(context->currentModule.top());
		auto& globals = context->globals.at(module);
		auto it = globals.find(name);
		if (it != globals.end()) {
			*it->second = value;
		} else {
			globals.insert({ std::string(name), wings::MakeRcPtr<Wg_Obj*>(value) });
		}
	}

	void Wg_RegisterModule(Wg_Context* context, const char* name, Wg_ModuleLoader loader) {
		WG_ASSERT_VOID(context && name && loader && wings::IsValidIdentifier(name));
		context->moduleLoaders.insert({ std::string(name), loader });
	}

	Wg_Obj* Wg_ImportModule(Wg_Context* context, const char* module, const char* alias) {
		WG_ASSERT(context && module && wings::IsValidIdentifier(module));
		if (alias) {
			WG_ASSERT(wings::IsValidIdentifier(alias));
		} else {
			alias = module;
		}

		if (!wings::LoadModule(context, module))
			return nullptr;

		Wg_Obj* moduleObject = Wg_Call(context->builtins.moduleObject, nullptr, 0);
		if (moduleObject == nullptr)
			return nullptr;
		auto& mod = context->globals.at(module);
		for (auto& [var, val] : mod) {
			Wg_SetAttribute(moduleObject, var.c_str(), *val);
		}
		Wg_SetGlobal(context, alias, moduleObject);
		return moduleObject;
	}

	Wg_Obj* Wg_ImportFromModule(Wg_Context* context, const char* module, const char* name, const char* alias) {
		WG_ASSERT(context && module && name && wings::IsValidIdentifier(module));
		if (alias) {
			WG_ASSERT(wings::IsValidIdentifier(alias));
		} else {
			alias = name;
		}

		if (!wings::LoadModule(context, module))
			return nullptr;

		auto& mod = context->globals.at(module);
		auto it = mod.find(name);
		if (it == mod.end()) {
			std::string msg = std::string("Cannot import '") + name
				+ "' from '" + module + "'";
			Wg_RaiseException(context, WG_EXC_IMPORTERROR, msg.c_str());
			return nullptr;
		}

		Wg_SetGlobal(context, alias, *it->second);
		return *it->second;
	}

	bool Wg_ImportAllFromModule(Wg_Context* context, const char* module) {
		WG_ASSERT(context && module && wings::IsValidIdentifier(module));

		if (!wings::LoadModule(context, module))
			return false;

		auto& mod = context->globals.at(module);
		for (auto& [var, val] : mod) {
			Wg_SetGlobal(context, var.c_str(), *val);
		}
		return true;
	}

	Wg_Obj* Wg_None(Wg_Context* context) {
		WG_ASSERT(context);
		return context->builtins.none;
	}

	Wg_Obj* Wg_NewBool(Wg_Context* context, bool value) {
		WG_ASSERT(context);
		if (value && context->builtins._true) {
			return context->builtins._true;
		} else if (!value && context->builtins._false) {
			return context->builtins._false;
		} else {
			return value ? context->builtins._true : context->builtins._false;
		}
	}

	Wg_Obj* Wg_NewInt(Wg_Context* context, Wg_int value) {
		WG_ASSERT(context);
		if (Wg_Obj* v = Wg_Call(context->builtins._int, nullptr, 0)) {
			v->Get<Wg_int>() = value;
			return v;
		} else {
			return nullptr;
		}
	}

	Wg_Obj* Wg_NewFloat(Wg_Context* context, Wg_float value) {
		WG_ASSERT(context);
		if (Wg_Obj* v = Wg_Call(context->builtins._float, nullptr, 0)) {
			v->Get<Wg_float>() = value;
			return v;
		} else {
			return nullptr;
		}
	}

	Wg_Obj* Wg_NewString(Wg_Context* context, const char* value) {
		WG_ASSERT(context);
		if (Wg_Obj* v = Wg_Call(context->builtins.str, nullptr, 0)) {
			v->Get<std::string>() = value ? value : "";
			return v;
		} else {
			return nullptr;
		}
	}

	Wg_Obj* Wg_NewStringBuffer(Wg_Context* context, const char* buffer, int length) {
		WG_ASSERT(context && buffer && length >= 0);
		if (Wg_Obj* v = Wg_Call(context->builtins.str, nullptr, 0)) {
			v->Get<std::string>() = std::string(buffer, length);
			return v;
		} else {
			return nullptr;
		}
	}

	Wg_Obj* Wg_NewTuple(Wg_Context* context, Wg_Obj** argv, int argc) {
		std::vector<wings::Wg_ObjRef> refs;
		WG_ASSERT(context && argc >= 0);
		if (argc > 0) {
			WG_ASSERT(argv);
			for (int i = 0; i < argc; i++) {
				refs.emplace_back(argv[i]);
				WG_ASSERT(argv[i]);
			}
		}

		if (Wg_Obj* v = Wg_Call(context->builtins.tuple, nullptr, 0)) {
			v->Get<std::vector<Wg_Obj*>>() = std::vector<Wg_Obj*>(argv, argv + argc);
			return v;
		} else {
			return nullptr;
		}
	}

	Wg_Obj* Wg_NewList(Wg_Context* context, Wg_Obj** argv, int argc) {
		std::vector<wings::Wg_ObjRef> refs;
		WG_ASSERT(context && argc >= 0);
		if (argc > 0) {
			WG_ASSERT(argv);
			for (int i = 0; i < argc; i++) {
				refs.emplace_back(argv[i]);
				WG_ASSERT(argv[i]);
			}
		}

		if (Wg_Obj* v = Wg_Call(context->builtins.list, nullptr, 0)) {
			v->Get<std::vector<Wg_Obj*>>() = std::vector<Wg_Obj*>(argv, argv + argc);
			return v;
		} else {
			return nullptr;
		}
	}

	Wg_Obj* Wg_NewDictionary(Wg_Context* context, Wg_Obj** keys, Wg_Obj** values, int argc) {
		std::vector<wings::Wg_ObjRef> refs;
		WG_ASSERT(context && argc >= 0);
		if (argc > 0) {
			WG_ASSERT(keys && values);
			for (int i = 0; i < argc; i++) {
				refs.emplace_back(keys[i]);
				refs.emplace_back(values[i]);
				WG_ASSERT(keys[i] && values[i]);
			}
		}

		// Pass a dummy kwargs to prevent stack overflow from recursion
		Wg_Obj* dummyKwargs = wings::Alloc(context);
		if (dummyKwargs == nullptr)
			return nullptr;
		dummyKwargs->type = "__map";
		wings::WDict wd{};
		dummyKwargs->data = &wd;

		if (Wg_Obj* v = Wg_Call(context->builtins.dict, nullptr, 0, dummyKwargs)) {
			for (int i = 0; i < argc; i++) {
				refs.emplace_back(v);
				try {
					v->Get<wings::WDict>()[keys[i]] = values[i];
				} catch (wings::HashException&) {
					return nullptr;
				}
			}
			return v;
		} else {
			return nullptr;
		}
	}

	Wg_Obj* Wg_NewSet(Wg_Context* context, Wg_Obj** argv, int argc) {
		std::vector<wings::Wg_ObjRef> refs;
		WG_ASSERT(context && argc >= 0);
		if (argc > 0) {
			WG_ASSERT(argv);
			for (int i = 0; i < argc; i++) {
				refs.emplace_back(argv[i]);
				WG_ASSERT(argv[i]);
			}
		}

		if (Wg_Obj* v = Wg_Call(context->builtins.set, nullptr, 0, nullptr)) {
			for (int i = 0; i < argc; i++) {
				try {
					v->Get<wings::WSet>().insert(argv[i]);
				} catch (wings::HashException&) {
					return nullptr;
				}
			}
			return v;
		} else {
			return nullptr;
		}
	}

	Wg_Obj* Wg_NewFunction(Wg_Context* context, Wg_Function fptr, void* userdata, const char* prettyName) {
		WG_ASSERT(context && fptr);

		Wg_Obj* obj = wings::Alloc(context);
		if (obj == nullptr)
			return nullptr;

		obj->attributes = context->builtins.func->Get<Wg_Obj::Class>().instanceAttributes.Copy();
		obj->type = "__func";
		auto data = new Wg_Obj::Func;
		Wg_SetUserdata(obj, data);
		Wg_RegisterFinalizer(obj, [](void* ud) { delete (Wg_Obj::Func*)ud; }, data);

		data->fptr = fptr;
		data->userdata = userdata;
		data->isMethod = false;
		data->module = context->currentModule.top();
		data->prettyName = prettyName ? prettyName : wings::DEFAULT_FUNC_NAME;
		data->self = nullptr;

		return obj;
	}

	Wg_Obj* Wg_BindMethod(Wg_Obj* klass, const char* name, Wg_Function fptr, void* userdata) {
		WG_ASSERT(klass && fptr && Wg_IsClass(klass));
		Wg_Context* context = klass->context;
		wings::Wg_ObjRef ref(klass);
		Wg_Obj* fn = Wg_NewFunction(context, fptr, userdata, name);
		if (fn == nullptr)
			return nullptr;
		fn->Get<Wg_Obj::Func>().isMethod = true;
		klass->Get<Wg_Obj::Class>().instanceAttributes.Set(name, fn);
		return fn;
	}

	Wg_Obj* Wg_NewClass(Wg_Context* context, const char* name, Wg_Obj** bases, int basesLen) {
		std::vector<wings::Wg_ObjRef> refs;
		WG_ASSERT(context && name && basesLen >= 0);
		if (basesLen > 0) {
			WG_ASSERT(bases);
			for (int i = 0; i < basesLen; i++) {
				WG_ASSERT(bases[i] && Wg_IsClass(bases[i]));
				refs.emplace_back(bases[i]);
			}
		}

		// Allocate class
		Wg_Obj* klass = wings::Alloc(context);
		if (klass == nullptr) {
			return nullptr;
		}
		refs.emplace_back(klass);
		klass->type = "__class";
		klass->data = new Wg_Obj::Class{ std::string(name) };
		Wg_RegisterFinalizer(klass, [](void* ud) { delete (Wg_Obj::Class*)ud; }, klass->data);
		klass->Get<Wg_Obj::Class>().module = context->currentModule.top();
		klass->Get<Wg_Obj::Class>().instanceAttributes.Set("__class__", klass);
		klass->attributes.AddParent(context->builtins.object->Get<Wg_Obj::Class>().instanceAttributes);

		// Set bases
		int actualBaseCount = basesLen ? basesLen : 1;
		Wg_Obj** actualBases = basesLen ? bases : &context->builtins.object;
		for (int i = 0; i < actualBaseCount; i++) {
			klass->Get<Wg_Obj::Class>().instanceAttributes.AddParent(actualBases[i]->Get<Wg_Obj::Class>().instanceAttributes);
			klass->Get<Wg_Obj::Class>().bases.push_back(actualBases[i]);
		}
		if (Wg_Obj* basesTuple = Wg_NewTuple(context, actualBases, actualBaseCount)) {
			klass->attributes.Set("__bases__", basesTuple);
		} else {
			return nullptr;
		}

		// Set construction function. This function forwards to __init__().
		klass->Get<Wg_Obj::Class>().userdata = klass;
		klass->Get<Wg_Obj::Class>().ctor = [](Wg_Context* context, Wg_Obj** argv, int argc) -> Wg_Obj* {
			Wg_Obj* _classObj = (Wg_Obj*)Wg_GetFunctionUserdata(context);

			Wg_Obj* instance = wings::Alloc(context);
			if (instance == nullptr)
				return nullptr;
			wings::Wg_ObjRef ref(instance);

			instance->attributes = _classObj->Get<Wg_Obj::Class>().instanceAttributes.Copy();
			instance->type = _classObj->Get<Wg_Obj::Class>().name;

			if (Wg_Obj* init = Wg_HasAttribute(instance, "__init__")) {
				if (Wg_IsFunction(init)) {
					Wg_Obj* kwargs = Wg_GetKwargs(context);
					if (kwargs == nullptr)
						return nullptr;

					Wg_Obj* ret = Wg_Call(init, argv, argc, kwargs);
					if (ret == nullptr) {
						return nullptr;
					} else if (!Wg_IsNone(ret)) {
						Wg_RaiseException(context, WG_EXC_TYPEERROR, "__init__() returned a non NoneType type");
						return nullptr;
					}
				}
			}

			return instance;
		};

		// Set init method
		auto init = [](Wg_Context* context, Wg_Obj** argv, int argc) -> Wg_Obj* {
			Wg_Obj* klass = (Wg_Obj*)Wg_GetFunctionUserdata(context);
			if (argc < 1) {
				Wg_RaiseArgumentCountError(klass->context, argc, -1);
				return nullptr;
			}

			const auto& bases = klass->Get<Wg_Obj::Class>().bases;
			if (bases.empty())
				return nullptr;

			if (Wg_Obj* baseInit = Wg_GetAttributeFromBase(argv[0], "__init__", bases[0])) {
				Wg_Obj* kwargs = Wg_GetKwargs(context);
				if (kwargs == nullptr)
					return nullptr;

				Wg_Obj* ret = Wg_Call(baseInit, argv + 1, argc - 1, kwargs);
				if (ret == nullptr) {
					return nullptr;
				} else if (!Wg_IsNone(ret)) {
					Wg_RaiseException(context, WG_EXC_TYPEERROR, "__init__() returned a non NoneType type");
					return nullptr;
				}
			}

			return Wg_None(context);
		};
		std::string initName = std::string(name) + ".__init__";

		wings::Wg_ObjRef ref(klass);
		Wg_Obj* initFn = Wg_BindMethod(klass, initName.c_str(), init, klass);
		if (initFn == nullptr)
			return nullptr;

		Wg_IncRef(klass);
		Wg_RegisterFinalizer(initFn, [](void* ud) { Wg_DecRef((Wg_Obj*)ud); }, klass);

		return klass;
	}

	bool Wg_IsNone(const Wg_Obj* obj) {
		WG_ASSERT(obj);
		return obj == obj->context->builtins.none;
	}

	bool Wg_IsBool(const Wg_Obj* obj) {
		WG_ASSERT(obj);
		return obj == obj->context->builtins._true
			|| obj == obj->context->builtins._false;
	}

	bool Wg_IsInt(const Wg_Obj* obj) {
		WG_ASSERT(obj);
		return obj->type == "__int";
	}

	bool Wg_IsIntOrFloat(const Wg_Obj* obj) {
		WG_ASSERT(obj);
		return obj->type == "__int" || obj->type == "__float";
	}

	bool Wg_IsString(const Wg_Obj* obj) {
		WG_ASSERT(obj);
		return obj->type == "__str";
	}

	bool Wg_IsTuple(const Wg_Obj* obj) {
		WG_ASSERT(obj);
		return obj->type == "__tuple";
	}

	bool Wg_IsList(const Wg_Obj* obj) {
		WG_ASSERT(obj);
		return obj->type == "__list";
	}

	bool Wg_IsDictionary(const Wg_Obj* obj) {
		WG_ASSERT(obj);
		return obj->type == "__map";
	}

	bool Wg_IsSet(const Wg_Obj* obj) {
		WG_ASSERT(obj);
		return obj->type == "__set";
	}

	bool Wg_IsClass(const Wg_Obj* obj) {
		WG_ASSERT(obj);
		return obj->type == "__class";
	}

	bool Wg_IsFunction(const Wg_Obj* obj) {
		WG_ASSERT(obj);
		return obj->type == "__func";
	}

	bool Wg_GetBool(const Wg_Obj* obj) {
		WG_ASSERT(obj && Wg_IsBool(obj));
		return obj->Get<bool>();
	}

	Wg_int Wg_GetInt(const Wg_Obj* obj) {
		WG_ASSERT(obj && Wg_IsInt(obj));
		return obj->Get<Wg_int>();
	}

	Wg_float Wg_GetFloat(const Wg_Obj* obj) {
		WG_ASSERT(obj && Wg_IsIntOrFloat(obj));
		if (Wg_IsInt(obj)) return (Wg_float)obj->Get<Wg_int>();
		else return obj->Get<Wg_float>();
	}

	const char* Wg_GetString(const Wg_Obj* obj, int* len) {
		WG_ASSERT(obj && Wg_IsString(obj));
		const auto& s = obj->Get<std::string>();
		if (len)
			*len = (int)s.size();
		return s.c_str();
	}

	void Wg_SetUserdata(Wg_Obj* obj, void* userdata) {
		WG_ASSERT_VOID(obj);
		obj->data = userdata;
	}

	bool Wg_TryGetUserdata(const Wg_Obj* obj, const char* type, void** out) {
		WG_ASSERT(obj && type);
		if (obj->type == std::string(type)) {
			if (out)
				*out = obj->data;
			return true;
		} else {
			return false;
		}
	}

	void Wg_RegisterFinalizer(Wg_Obj* obj, Wg_Finalizer finalizer, void* userdata) {
		WG_ASSERT_VOID(obj && finalizer);
		obj->finalizers.push_back({ finalizer, userdata });
	}

	Wg_Obj* Wg_HasAttribute(Wg_Obj* obj, const char* attribute) {
		WG_ASSERT(obj && attribute && wings::IsValidIdentifier(attribute));
		Wg_Obj* mem = obj->attributes.Get(attribute);
		if (mem && Wg_IsFunction(mem) && mem->Get<Wg_Obj::Func>().isMethod) {
			mem->Get<Wg_Obj::Func>().self = obj;
		}
		return mem;
	}

	Wg_Obj* Wg_GetAttribute(Wg_Obj* obj, const char* attribute) {
		WG_ASSERT(obj && attribute && wings::IsValidIdentifier(attribute));
		Wg_Obj* mem = obj->attributes.Get(attribute);
		if (mem == nullptr) {
			Wg_RaiseAttributeError(obj, attribute);
		} else if (Wg_IsFunction(mem) && mem->Get<Wg_Obj::Func>().isMethod) {
			mem->Get<Wg_Obj::Func>().self = obj;
		}
		return mem;
	}

	void Wg_SetAttribute(Wg_Obj* obj, const char* attribute, Wg_Obj* value) {
		WG_ASSERT_VOID(obj && attribute && value && wings::IsValidIdentifier(attribute));
		obj->attributes.Set(attribute, value);
	}

	Wg_Obj* Wg_GetAttributeFromBase(Wg_Obj* obj, const char* attribute, Wg_Obj* baseClass) {
		WG_ASSERT(obj && attribute && wings::IsValidIdentifier(attribute));

		Wg_Obj* mem{};
		if (baseClass == nullptr) {
			mem = obj->attributes.GetFromBase(attribute);
		} else {
			mem = baseClass->Get<Wg_Obj::Class>().instanceAttributes.Get(attribute);
		}

		if (mem && Wg_IsFunction(mem) && mem->Get<Wg_Obj::Func>().isMethod) {
			mem->Get<Wg_Obj::Func>().self = obj;
		}
		return mem;
	}

	Wg_Obj* Wg_IsInstance(const Wg_Obj* instance, Wg_Obj* const* types, int typesLen) {
		WG_ASSERT(instance && typesLen >= 0 && (types || typesLen == 0));
		for (int i = 0; i < typesLen; i++)
			WG_ASSERT(types[i] && Wg_IsClass(types[i]));

		// Cannot use Wg_HasAttribute here because instance is a const pointer
		Wg_Obj* klass = instance->attributes.Get("__class__");
		if (klass == nullptr)
			return nullptr;
		wings::Wg_ObjRef ref(klass);

		std::queue<wings::Wg_ObjRef> toCheck;
		toCheck.emplace(klass);

		while (!toCheck.empty()) {
			auto end = types + typesLen;
			auto it = std::find(types, end, toCheck.front().Get());
			if (it != end)
				return *it;

			Wg_Obj* bases = Wg_HasAttribute(toCheck.front().Get(), "__bases__");
			if (bases && Wg_IsTuple(bases))
				for (Wg_Obj* base : bases->Get<std::vector<Wg_Obj*>>())
					toCheck.emplace(base);

			toCheck.pop();
		}
		return nullptr;
	}

	bool Wg_Iterate(Wg_Obj* obj, void* userdata, Wg_IterationCallback callback) {
		WG_ASSERT(obj && callback);
		Wg_Context* context = obj->context;

		wings::Wg_ObjRef objRef(obj);

		Wg_Obj* iter = Wg_CallMethod(obj, "__iter__", nullptr, 0);
		if (iter == nullptr)
			return false;
		wings::Wg_ObjRef iterRef(iter);

		while (true) {
			Wg_Obj* yielded = Wg_CallMethod(iter, "__next__", nullptr, 0);

			Wg_Obj* exc = Wg_GetException(context);
			if (exc) {
				if (Wg_IsInstance(exc, &context->builtins.stopIteration, 1)) {
					Wg_ClearException(context);
					return true;
				} else {
					return false;
				}
			}

			WG_ASSERT(yielded); // If no exception was thrown then a value must be yielded
			wings::Wg_ObjRef yieldedRef(yielded);
			if (!callback(yielded, userdata))
				return Wg_GetException(context) == nullptr;

			if (Wg_GetException(context))
				return false;
		}
	}

	bool Wg_Unpack(Wg_Obj* obj, int count, Wg_Obj** out) {
		WG_ASSERT(obj && (count == 0 || out));

		Wg_Context* context = obj->context;
		struct State {
			Wg_Context* context;
			Wg_Obj** array;
			int count;
			int index;
		} s = { context, out, count, 0 };

		bool success = Wg_Iterate(obj, &s, [](Wg_Obj* yielded, void* userdata) {
			State* s = (State*)userdata;
			if (s->index >= s->count) {
				Wg_RaiseException(s->context, WG_EXC_VALUEERROR, "Too many values to unpack");
			} else {
				Wg_IncRef(yielded);
				s->array[s->index] = yielded;
				s->index++;
			}
			return true;
			});

		for (int i = s.index; i; i--)
			Wg_DecRef(out[i - 1]);

		if (!success) {
			return false;
		} else if (s.index < count) {
			Wg_RaiseException(context, WG_EXC_VALUEERROR, "Not enough values to unpack");
			return false;
		} else {
			return true;
		}
	}

	Wg_Obj* Wg_GetKwargs(Wg_Context* context) {
		WG_ASSERT(context && !context->kwargs.empty());
		if (context->kwargs.back() == nullptr) {
			context->kwargs.back() = Wg_NewDictionary(context);
		}
		return context->kwargs.back();
	}

	void* Wg_GetFunctionUserdata(Wg_Context* context) {
		WG_ASSERT(context && !context->kwargs.empty());
		return context->userdata.back();
	}

	Wg_Obj* Wg_Call(Wg_Obj* callable, Wg_Obj** argv, int argc, Wg_Obj* kwargsDict) {
		WG_ASSERT(callable && argc >= 0 && (argc == 0 || argv));
		if (argc)
			WG_ASSERT(argv);
		for (int i = 0; i < argc; i++)
			WG_ASSERT(argv[i]);

		Wg_Context* context = callable->context;
		
		// Check recursion limit
		if (context->kwargs.size() >= (size_t)context->config.maxRecursion) {
			Wg_RaiseException(context, WG_EXC_RECURSIONERROR);
			return nullptr;
		}

		// Call the __call__ method if object is neither a function nor a class
		if (!Wg_IsFunction(callable) && !Wg_IsClass(callable)) {
			return Wg_CallMethod(callable, "__call__", argv, argc);
		}

		// Validate keyword arguments
		if (kwargsDict) {
			if (!Wg_IsDictionary(kwargsDict)) {
				Wg_RaiseException(context, WG_EXC_TYPEERROR, "Keyword arguments must be a dictionary");
				return nullptr;
			}
			for (const auto& [key, value] : kwargsDict->Get<wings::WDict>()) {
				if (!Wg_IsString(key)) {
					Wg_RaiseException(context, WG_EXC_TYPEERROR, "Keyword arguments dictionary must only contain string keys");
					return nullptr;
				}
			}
		}

		// Prevent arguments from being garbage collected
		std::vector<wings::Wg_ObjRef> refs;
		refs.emplace_back(callable);
		for (int i = 0; i < argc; i++)
			refs.emplace_back(argv[i]);

		// Get the raw function pointer, userdata, module, and self
		// depending on whether the callable is a function or class.
		Wg_Obj* (*fptr)(Wg_Context*, Wg_Obj**, int);
		void* userdata = nullptr;
		std::string_view module;
		Wg_Obj* self = nullptr;
		if (Wg_IsFunction(callable)) {
			const auto& func = callable->Get<Wg_Obj::Func>();
			if (func.self)
				self = func.self;
			fptr = func.fptr;
			userdata = func.userdata;
			module = func.module;
		} else {
			const auto& klass = callable->Get<Wg_Obj::Class>();
			fptr = klass.ctor;
			userdata = klass.userdata;
			module = klass.module;
		}

		// Prepare arguments into a contiguous buffer
		std::vector<Wg_Obj*> argsWithSelf;
		if (self) {
			argsWithSelf.push_back(self);
			refs.emplace_back(self);
		}
		argsWithSelf.insert(argsWithSelf.end(), argv, argv + argc);

		// Push various data onto stacks
		context->currentModule.push(module);
		context->userdata.push_back(userdata);
		context->kwargs.push_back(kwargsDict);
		if (Wg_IsFunction(callable)) {
			const auto& func = callable->Get<Wg_Obj::Func>();
			context->currentTrace.push_back(wings::TraceFrame{
				{},
				"",
				func.module,
				func.prettyName
				});
		}
		
		// Perform the call
		Wg_Obj* ret = nullptr;
		try {
			ret = fptr(context, argsWithSelf.data(), (int)argsWithSelf.size());
		} catch (std::bad_alloc&) {
			Wg_RaiseException(context, WG_EXC_MEMORYERROR);
		}
		
		// Pop the data off the stacks
		context->currentModule.pop();
		context->userdata.pop_back();
		context->kwargs.pop_back();
		if (Wg_IsFunction(callable)) {
			context->currentTrace.pop_back();
		}

		return ret;
	}

	Wg_Obj* Wg_CallMethod(Wg_Obj* obj, const char* member, Wg_Obj** argv, int argc, Wg_Obj* kwargsDict) {
		WG_ASSERT(obj && member && wings::IsValidIdentifier(member));
		if (argc)
			WG_ASSERT(argv);
		for (int i = 0; i < argc; i++)
			WG_ASSERT(argv[i]);

		if (Wg_Obj* method = Wg_GetAttribute(obj, member)) {
			return Wg_Call(method, argv, argc, kwargsDict);
		} else {
			return nullptr;
		}
	}

	Wg_Obj* Wg_CallMethodFromBase(Wg_Obj* obj, const char* member, Wg_Obj** argv, int argc, Wg_Obj* kwargsDict, Wg_Obj* baseClass) {
		WG_ASSERT(obj && member && wings::IsValidIdentifier(member));
		if (argc)
			WG_ASSERT(argv);
		for (int i = 0; i < argc; i++)
			WG_ASSERT(argv[i]);

		if (Wg_Obj* method = Wg_GetAttributeFromBase(obj, member, baseClass)) {
			return Wg_Call(method, argv, argc, kwargsDict);
		} else {
			Wg_RaiseAttributeError(obj, member);
			return nullptr;
		}
	}

	bool Wg_ParseKwargs(Wg_Obj* kwargs, const char* const* keys, int count, Wg_Obj** out) {
		WG_ASSERT(kwargs && keys && out && count > 0 && Wg_IsDictionary(kwargs));

		wings::Wg_ObjRef ref(kwargs);
		auto& buf = kwargs->Get<wings::WDict>();
		for (int i = 0; i < count; i++) {
			Wg_Obj* key = Wg_NewString(kwargs->context, keys[i]);
			if (key == nullptr)
				return false;

			wings::WDict::iterator it;
			try {
				it = buf.find(key);
			} catch (wings::HashException&) {
				return false;
			}

			if (it != buf.end()) {
				out[i] = it->second;
			} else {
				out[i] = nullptr;
			}
		}
		return true;
	}

	Wg_Obj* Wg_GetIndex(Wg_Obj* obj, Wg_Obj* index) {
		WG_ASSERT(obj && index);
		return Wg_CallMethod(obj, "__getitem__", &index, 1);
	}

	Wg_Obj* Wg_SetIndex(Wg_Obj* obj, Wg_Obj* index, Wg_Obj* value) {
		WG_ASSERT(obj && index && value);
		Wg_Obj* argv[2] = { index, value };
		return Wg_CallMethod(obj, "__setitem__", argv, 2);
	}

	Wg_Obj* Wg_UnaryOp(Wg_UnOp op, Wg_Obj* arg) {
		WG_ASSERT(arg);
		Wg_Context* context = arg->context;
		switch (op) {
		case WG_UOP_POS:
			return Wg_CallMethod(arg, "__pos__", nullptr, 0);
		case WG_UOP_NEG:
			return Wg_CallMethod(arg, "__neg__", nullptr, 0);
		case WG_UOP_BITNOT:
			return Wg_CallMethod(arg, "__invert__", nullptr, 0);
		case WG_UOP_HASH:
			return Wg_Call(context->builtins.hash, &arg, 1);
		case WG_UOP_LEN:
			return Wg_Call(context->builtins.len, &arg, 1);
		case WG_UOP_BOOL:
			if (Wg_IsBool(arg))
				return arg;
			return Wg_Call(context->builtins._bool, &arg, 1);
		case WG_UOP_INT:
			if (Wg_IsInt(arg))
				return arg;
			return Wg_Call(context->builtins._int, &arg, 1);
		case WG_UOP_FLOAT:
			if (Wg_IsIntOrFloat(arg))
				return arg;
			return Wg_Call(context->builtins._float, &arg, 1);
		case WG_UOP_STR:
			if (Wg_IsString(arg))
				return arg;
			return Wg_Call(context->builtins.str, &arg, 1);
		case WG_UOP_REPR:
			return Wg_Call(context->builtins.repr, &arg, 1);
		case WG_UOP_INDEX: {
			Wg_Obj* index = Wg_CallMethod(arg, "__index__", nullptr, 0);
			if (index == nullptr) {
				return nullptr;
			} else if (!Wg_IsInt(index)) {
				Wg_RaiseException(context, WG_EXC_TYPEERROR, "__index__() returned a non integer type");
				return nullptr;
			} else {
				return index;
			}
		}
		default:
			WG_UNREACHABLE();
		}
	}

	static const std::unordered_map<Wg_BinOp, const char*> OP_METHOD_NAMES = {
		{ WG_BOP_ADD, "__add__" },
		{ WG_BOP_SUB, "__sub__" },
		{ WG_BOP_MUL, "__mul__" },
		{ WG_BOP_DIV, "__truediv__" },
		{ WG_BOP_FLOORDIV, "__floordiv__" },
		{ WG_BOP_MOD, "__mod__" },
		{ WG_BOP_POW, "__pow__" },
		{ WG_BOP_BITAND, "__and__" },
		{ WG_BOP_BITOR, "__or__" },
		{ WG_BOP_BITXOR, "__not__" },
		{ WG_BOP_SHL, "__lshift__" },
		{ WG_BOP_SHR, "__rshift__" },
		{ WG_BOP_IN, "__contains__" },
		{ WG_BOP_EQ, "__eq__" },
		{ WG_BOP_NE, "__ne__" },
		{ WG_BOP_LT, "__lt__" },
		{ WG_BOP_LE, "__le__" },
		{ WG_BOP_GT, "__gt__" },
		{ WG_BOP_GE, "__ge__" },
	};

	Wg_Obj* Wg_BinaryOp(Wg_BinOp op, Wg_Obj* lhs, Wg_Obj* rhs) {
		WG_ASSERT(lhs && rhs);

		if (op == WG_BOP_IN)
			std::swap(lhs, rhs);

		auto method = OP_METHOD_NAMES.find(op);

		switch (op) {
		case WG_BOP_ADD:
		case WG_BOP_SUB:
		case WG_BOP_MUL:
		case WG_BOP_DIV:
		case WG_BOP_FLOORDIV:
		case WG_BOP_MOD:
		case WG_BOP_POW:
		case WG_BOP_BITAND:
		case WG_BOP_BITOR:
		case WG_BOP_BITXOR:
		case WG_BOP_SHL:
		case WG_BOP_SHR:
			return Wg_CallMethod(lhs, method->second, &rhs, 1);
		case WG_BOP_EQ:
		case WG_BOP_NE:
		case WG_BOP_LT:
		case WG_BOP_LE:
		case WG_BOP_GT:
		case WG_BOP_GE:
		case WG_BOP_IN: {
			Wg_Obj* boolResult = Wg_CallMethod(lhs, method->second, &rhs, 1);
			if (!Wg_IsBool(boolResult)) {
				std::string message = method->second;
				message += "() returned a non bool type";
				Wg_RaiseException(boolResult->context, WG_EXC_TYPEERROR, message.c_str());
				return nullptr;
			}
			return boolResult;
		}
		case WG_BOP_NOTIN:
			if (Wg_Obj* in = Wg_BinaryOp(WG_BOP_IN, lhs, rhs)) {
				return Wg_UnaryOp(WG_UOP_NOT, in);
			} else {
				return nullptr;
			}
		case WG_BOP_AND: {
			Wg_Obj* lhsb = Wg_UnaryOp(WG_UOP_BOOL, lhs);
			if (lhsb == nullptr)
				return nullptr;
			if (!Wg_GetBool(lhsb))
				return lhsb;
			return Wg_UnaryOp(WG_UOP_BOOL, rhs);
		}
		case WG_BOP_OR: {
			Wg_Obj* lhsb = Wg_UnaryOp(WG_UOP_BOOL, lhs);
			if (lhsb == nullptr)
				return nullptr;
			if (Wg_GetBool(lhsb))
				return lhsb;
			return Wg_UnaryOp(WG_UOP_BOOL, rhs);
		}
		default:
			WG_UNREACHABLE();
		}
	}

	const char* Wg_GetErrorMessage(Wg_Context* context) {
		WG_ASSERT(context);

		if (context->currentException == nullptr) {
			return (context->traceMessage = "Ok").c_str();
		}

		std::stringstream ss;
		ss << "Traceback (most recent call last):\n";

		for (const auto& frame : context->exceptionTrace) {
			//if (frame.module == "__builtins__" ||
			//	frame.module == "math" ||
			//	frame.module == "random"
			//	)
			//{
			//	continue;
			//}

			ss << "  ";
			bool written = false;
			
			ss << "Module " << frame.module;
			written = true;

			if (frame.srcPos.line != (size_t)-1) {
				if (written) ss << ", ";
				ss << "Line " << (frame.srcPos.line + 1);
				written = true;
			}

			if (frame.func != wings::DEFAULT_FUNC_NAME) {
				if (written) ss << ", ";
				ss << "Function " << frame.func << "()";
			}

			ss << "\n";

			if (!frame.lineText.empty()) {
				std::string lineText = frame.lineText;
				std::replace(lineText.begin(), lineText.end(), '\t', ' ');

				size_t skip = lineText.find_first_not_of(' ');
				ss << "    " << (lineText.c_str() + skip) << "\n";
				if (frame.syntaxError && skip <= frame.srcPos.column)
				    ss << std::string(frame.srcPos.column + 4 - skip, ' ') << "^\n";
			}
		}

		ss << context->currentException->type;
		if (Wg_Obj* msg = Wg_HasAttribute(context->currentException, "_message"))
			if (Wg_IsString(msg) && *Wg_GetString(msg))
				ss << ": " << Wg_GetString(msg);
		ss << "\n";

		context->traceMessage = ss.str();
		return context->traceMessage.c_str();
	}

	Wg_Obj* Wg_GetException(Wg_Context* context) {
		WG_ASSERT(context);
		return context->currentException;
	}

	void Wg_ClearException(Wg_Context* context) {
		WG_ASSERT_VOID(context);
		context->currentException = nullptr;
		context->exceptionTrace.clear();
		//context->currentTrace.clear();
		context->traceMessage.clear();
	}

	void Wg_RaiseException(Wg_Context* context, Wg_Exc type, const char* message) {
		WG_ASSERT_VOID(context);
		switch (type) {
		case WG_EXC_BASEEXCEPTION:
			return Wg_RaiseExceptionClass(context->builtins.baseException, message);
		case WG_EXC_SYSTEMEXIT:
			return Wg_RaiseExceptionClass(context->builtins.systemExit, message);
		case WG_EXC_EXCEPTION:
			return Wg_RaiseExceptionClass(context->builtins.exception, message);
		case WG_EXC_STOPITERATION:
			return Wg_RaiseExceptionClass(context->builtins.stopIteration, message);
		case WG_EXC_ARITHMETICERROR:
			return Wg_RaiseExceptionClass(context->builtins.arithmeticError, message);
		case WG_EXC_OVERFLOWERROR:
			return Wg_RaiseExceptionClass(context->builtins.overflowError, message);
		case WG_EXC_ZERODIVISIONERROR:
			return Wg_RaiseExceptionClass(context->builtins.zeroDivisionError, message);
		case WG_EXC_ATTRIBUTEERROR:
			return Wg_RaiseExceptionClass(context->builtins.attributeError, message);
		case WG_EXC_IMPORTERROR:
			return Wg_RaiseExceptionClass(context->builtins.importError, message);
		case WG_EXC_LOOKUPERROR:
			return Wg_RaiseExceptionClass(context->builtins.lookupError, message);
		case WG_EXC_INDEXERROR:
			return Wg_RaiseExceptionClass(context->builtins.indexError, message);
		case WG_EXC_KEYERROR:
			return Wg_RaiseExceptionClass(context->builtins.keyError, message);
		case WG_EXC_MEMORYERROR:
			return Wg_RaiseExceptionObject(context->builtins.memoryErrorInstance);
		case WG_EXC_NAMEERROR:
			return Wg_RaiseExceptionClass(context->builtins.nameError, message);
		case WG_EXC_OSERROR:
			return Wg_RaiseExceptionClass(context->builtins.osError, message);
		case WG_EXC_ISADIRECTORYERROR:
			return Wg_RaiseExceptionClass(context->builtins.isADirectoryError, message);
		case WG_EXC_RUNTIMEERROR:
			return Wg_RaiseExceptionClass(context->builtins.runtimeError, message);
		case WG_EXC_NOTIMPLEMENTEDERROR:
			return Wg_RaiseExceptionClass(context->builtins.notImplementedError, message);
		case WG_EXC_RECURSIONERROR:
			return Wg_RaiseExceptionObject(context->builtins.recursionErrorInstance);
		case WG_EXC_SYNTAXERROR:
			return Wg_RaiseExceptionClass(context->builtins.syntaxError, message);
		case WG_EXC_TYPEERROR:
			return Wg_RaiseExceptionClass(context->builtins.typeError, message);
		case WG_EXC_VALUEERROR:
			return Wg_RaiseExceptionClass(context->builtins.valueError, message);
		default:
			WG_ASSERT_VOID(false);
		}
	}

	void Wg_RaiseExceptionClass(Wg_Obj* klass, const char* message) {
		WG_ASSERT_VOID(klass);
		wings::Wg_ObjRef ref(klass);

		Wg_Obj* msg = Wg_NewString(klass->context, message);
		if (msg == nullptr) {
			return;
		}

		// If exception creation was successful then set the exception.
		// Otherwise the exception will already be set by some other code.
		if (Wg_Obj* exceptionObject = Wg_Call(klass, &msg, msg ? 1 : 0)) {
			Wg_RaiseExceptionObject(exceptionObject);
		}
	}

	void Wg_RaiseExceptionObject(Wg_Obj* obj) {
		WG_ASSERT_VOID(obj);
		Wg_Context* context = obj->context;
		if (Wg_IsInstance(obj, &context->builtins.baseException, 1)) {
			context->currentException = obj;
			context->exceptionTrace.clear();
			for (const auto& frame : context->currentTrace)
				context->exceptionTrace.push_back(frame.ToOwned());
		} else {
			Wg_RaiseException(context, WG_EXC_TYPEERROR, "exceptions must derive from BaseException");
		}
	}

	void Wg_RaiseArgumentCountError(Wg_Context* context, int given, int expected) {
		WG_ASSERT_VOID(context && given >= 0 && expected >= -1);
		std::string msg;
		if (expected != -1) {
			msg = "Function takes " +
				std::to_string(expected) +
				" argument(s) but " +
				std::to_string(given) +
				(given == 1 ? " was given" : " were given");
		} else {
			msg = "function does not take " +
				std::to_string(given) +
				" argument(s)";
		}
		Wg_RaiseException(context, WG_EXC_TYPEERROR, msg.c_str());
	}

	void Wg_RaiseArgumentTypeError(Wg_Context* context, int index, const char* expected) {
		WG_ASSERT_VOID(context && index >= 0 && expected);
		std::string msg = "Argument " + std::to_string(index + 1)
			+ " Expected type " + expected;
		Wg_RaiseException(context, WG_EXC_TYPEERROR, msg.c_str());
	}

	void Wg_RaiseAttributeError(const Wg_Obj* obj, const char* attribute) {
		WG_ASSERT_VOID(obj && attribute);
		std::string msg = "'" + wings::WObjTypeToString(obj) +
			"' object has no attribute '" + attribute + "'";
		Wg_RaiseException(obj->context, WG_EXC_ATTRIBUTEERROR, msg.c_str());
	}

	void Wg_RaiseZeroDivisionError(Wg_Context* context) {
		WG_ASSERT_VOID(context);
		Wg_RaiseException(context, WG_EXC_ZERODIVISIONERROR, "division by zero");
	}

	void Wg_RaiseIndexError(Wg_Context* context) {
		WG_ASSERT_VOID(context);
		Wg_RaiseException(context, WG_EXC_INDEXERROR, "index out of range");
	}

	void Wg_RaiseKeyError(Wg_Context* context, Wg_Obj* key) {
		WG_ASSERT_VOID(context);

		if (key == nullptr) {
			Wg_RaiseException(context, WG_EXC_KEYERROR);
		} else {
			std::string s = "<exception str() failed>";
			if (Wg_Obj* repr = Wg_UnaryOp(WG_UOP_REPR, key))
				s = Wg_GetString(repr);
			Wg_RaiseException(context, WG_EXC_KEYERROR, s.c_str());
		}
	}

	void Wg_RaiseNameError(Wg_Context* context, const char* name) {
		WG_ASSERT_VOID(context && name);
		std::string msg = "The name '";
		msg += name;
		msg += "' is not defined";
		Wg_RaiseException(context, WG_EXC_NAMEERROR, msg.c_str());
	}

	void Wg_CollectGarbage(Wg_Context* context) {
		WG_ASSERT_VOID(context);

		std::deque<const Wg_Obj*> inUse;
		if (!context->closing) {
			if (context->currentException)
				inUse.push_back(context->currentException);
			for (const auto& obj : context->mem)
				if (obj->refCount)
					inUse.push_back(obj.get());
			for (auto& [_, globals] : context->globals)
				for (auto& var : globals)
					inUse.push_back(*var.second);
			for (Wg_Obj* obj : context->kwargs)
				if (obj)
					inUse.push_back(obj);
			for (auto& obj : context->builtins.GetAll())
				if (obj)
					inUse.push_back(obj);
			if (context->argv)
				inUse.push_back(context->argv);
			for (const auto& executor : context->executors)
				executor->GetReferences(inUse);
		}

		// Recursively find objects in use
		std::unordered_set<const Wg_Obj*> traversed;
		while (inUse.size()) {
			auto obj = inUse.back();
			inUse.pop_back();
			if (!traversed.contains(obj)) {
				traversed.insert(obj);

				if (Wg_IsTuple(obj) || Wg_IsList(obj)) {
					inUse.insert(
						inUse.end(),
						obj->Get<std::vector<Wg_Obj*>>().begin(),
						obj->Get<std::vector<Wg_Obj*>>().end()
					);
				} else if (Wg_IsDictionary(obj)) {
					for (const auto& [key, value] : obj->Get<wings::WDict>()) {
						inUse.push_back(key);
						inUse.push_back(value);
					}
				} else if (Wg_IsSet(obj)) {
					for (Wg_Obj* value : obj->Get<wings::WSet>()) {
						inUse.push_back(value);
					}
				} else if (Wg_IsFunction(obj)) {
					const auto& fn = obj->Get<Wg_Obj::Func>();
					if (fn.self) {
						inUse.push_back(fn.self);
					}
					if (fn.fptr == &wings::DefObject::Run) {
						auto* def = (wings::DefObject*)fn.userdata;
						for (const auto& capture : def->captures)
							inUse.push_back(*capture.second);
						for (const auto& arg : def->defaultParameterValues)
							inUse.push_back(arg);
					}
				} else if (Wg_IsClass(obj)) {
					inUse.insert(
						inUse.end(),
						obj->Get<Wg_Obj::Class>().bases.begin(),
						obj->Get<Wg_Obj::Class>().bases.end()
					);
					obj->Get<Wg_Obj::Class>().instanceAttributes.ForEach([&](auto& entry) {
						inUse.push_back(entry);
						});
				}

				obj->attributes.ForEach([&](auto& entry) {
					inUse.push_back(entry);
					});
			}
		}

		// Call finalizers
		for (auto& obj : context->mem)
			if (!traversed.contains(obj.get()))
				for (const auto& finalizer : obj->finalizers)
					finalizer.first(finalizer.second);

		// Remove unused objects
		context->mem.erase(
			std::remove_if(
				context->mem.begin(),
				context->mem.end(),
				[&traversed](const auto& obj) { return !traversed.contains(obj.get()); }
			),
			context->mem.end()
		);

		context->lastObjectCountAfterGC = context->mem.size();
	}

	void Wg_IncRef(Wg_Obj* obj) {
		WG_ASSERT_VOID(obj);
		obj->refCount++;
	}

	void Wg_DecRef(Wg_Obj* obj) {
		WG_ASSERT_VOID(obj && obj->refCount > 0);
		obj->refCount--;
	}

	Wg_Context* Wg_GetContextFromObject(Wg_Obj* obj) {
		WG_ASSERT(obj);
		return obj->context;
	}

} // extern "C"
