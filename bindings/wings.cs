using System;
using System.Text;
using System.Linq;
using System.Runtime.InteropServices;

namespace Wings {
	public static class Wg {
		public struct Context {
			public IntPtr _data;
			public static implicit operator bool(Context a) => a._data != IntPtr.Zero;
			public static bool operator==(Context a, Context b) => a._data == b._data;
			public static bool operator!=(Context a, Context b) => !(a == b);
			public override bool Equals(Object? a) => a is Context b && this == b;
			public override int GetHashCode() => _data.GetHashCode();
		}

		public struct Obj {
			public IntPtr _data;
			public static implicit operator bool(Obj a) => a._data != IntPtr.Zero;
			public static bool operator==(Obj a, Obj b) => a._data == b._data;
			public static bool operator!=(Obj a, Obj b) => !(a == b);
			public override bool Equals(Object? a) => a is Obj b && this == b;
			public override int GetHashCode() => _data.GetHashCode();
		}

		[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
		public delegate Obj Function(Context context, IntPtr argv, int argc);
		[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
		public delegate void Finalizer(IntPtr userdata);
		[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
		public delegate void PrintFunction(IntPtr message, int len, IntPtr userdata);
		[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
		public delegate void ErrorCallback(IntPtr message);
		[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
		public delegate void IterationCallback(Obj obj, IntPtr userdata);
		[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
		[return: MarshalAs(UnmanagedType.U1)]
		public delegate bool ModuleLoader(Context context);
		

		[StructLayout(LayoutKind.Sequential, Pack=1)]
		private struct Wg_ConfigNative {
			public byte enableOSAccess;
			public int maxAlloc;
			public int maxRecursion;
			public float gcRunFactor;
			public IntPtr print;
			public IntPtr printUserdata;
			public IntPtr importPath;
			public IntPtr argv;
			public int argc;
			public Wg_ConfigNative(Config src) {
				enableOSAccess = (byte)(src.enableOSAccess ? 1 : 0);
				maxAlloc = src.maxAlloc;
				maxRecursion = src.maxRecursion;
				gcRunFactor = src.gcRunFactor;
				print = src.print is null ? IntPtr.Zero : Marshal.GetFunctionPointerForDelegate(src.print);
				printUserdata = src.printUserdata;
				importPath = Marshal.StringToHGlobalAnsi(src.importPath);
				if (src.argv is null) {
					argv = default;
				} else {
					argv = Marshal.AllocHGlobal(src.argv.Length * IntPtr.Size);
					for (int i = 0; i < src.argv.Length; i++) {
						Marshal.WriteIntPtr(argv, i * IntPtr.Size, Marshal.StringToHGlobalAnsi(src.argv[i]));
					}
				}
				argc = src.argc;
			}
			public void Free(Config src) {
				Marshal.FreeHGlobal(importPath);
				if (src.argv != null) {
					for (int i = 0; i < src.argv.Length; i++) {
						Marshal.FreeHGlobal(Marshal.ReadIntPtr(argv, i * IntPtr.Size));
					}
					Marshal.FreeHGlobal(argv);
				}
			}
		}

		private static Config MakeWg_Config(Wg_ConfigNative src) {
			var dst = new Config();
			dst.enableOSAccess = src.enableOSAccess != 0;
			dst.maxAlloc = src.maxAlloc;
			dst.maxRecursion = src.maxRecursion;
			dst.gcRunFactor = src.gcRunFactor;
			dst.print = src.print == IntPtr.Zero ? null : Marshal.GetDelegateForFunctionPointer<PrintFunction>(src.print);
			dst.printUserdata = src.printUserdata;
			dst.importPath = null;
			dst.argv = null;
			dst.argc = src.argc;
			return dst;
		}
		public struct Config {
			/// <summary>
			/// Enables the os module and the global 'open' function.
			/// </summary>
			/// <remarks>
			/// Although this option can be enabled to prevent
			/// scripts from directly accessing OS resources,
			/// it does not provide a full sandbox. Untrusted scripts
			/// should never be run with or without this option enabled.
			/// </remarks>
			public bool enableOSAccess;
			/// <summary>
			/// The maximum number of objects allowed to be allocated
			/// at a time before a MemoryError will be raised.
			/// </summary>
			public int maxAlloc;
			/// <summary>
			/// The maximum recursion depth allowed before a RecursionError will be raised.
			/// </summary>
			public int maxRecursion;
			/// <summary>
			/// The 'aggressiveness' of the garbage collector. Higher means less aggressive.
			/// </summary>
			public float gcRunFactor;
			/// <summary>
			/// The callback to be invoked when print is called in the interpreter.
			/// If this is null, then print messages are discarded.
			/// </summary>
			/// <see>
			/// Print
			/// PrintString
			/// </see>
			public PrintFunction? print;
			/// <summary>
			/// The userdata passed to the print callback.
			/// </summary>
			public IntPtr printUserdata;
			/// <summary>
			/// The path to search in when importing file modules.
			/// The terminating directory separator is optional.
			/// </summary>
			public string? importPath;
			/// <summary>
			/// The commandline arguments passed to the interpreter.
			/// If argc is 0, then this can be null.
			/// </summary>
			public string[]? argv;
			/// <summary>
			/// The length of the argv array.
			/// If argc is 0, then a length 1 array with an empty string is implied.
			/// </summary>
			public int argc;
		}

		public enum UnOp {
			/// <summary>
			/// The identity operator
			/// </summary>
			POS,
			/// <summary>
			/// The unary minus operator
			/// </summary>
			NEG,
			/// <summary>
			/// The bitwise complement operator
			/// </summary>
			BITNOT,
			/// <summary>
			/// The logical not operator
			/// </summary>
			NOT,
			/// <summary>
			/// The hash operator
			/// </summary>
			HASH,
			/// <summary>
			/// The length operator
			/// </summary>
			LEN,
			/// <summary>
			/// The bool conversion operator
			/// </summary>
			BOOL,
			/// <summary>
			/// The integer conversion operator
			/// </summary>
			INT,
			/// <summary>
			/// The float operator operator
			/// </summary>
			FLOAT,
			/// <summary>
			/// The string conversion operator
			/// </summary>
			STR,
			/// <summary>
			/// The string representation operator
			/// </summary>
			REPR,
			/// <summary>
			/// The index conversion operator
			/// </summary>
			INDEX,
		}

		public enum BinOp {
			/// <summary>
			/// The addition operator
			/// </summary>
			ADD,
			/// <summary>
			/// The subtraction operator
			/// </summary>
			SUB,
			/// <summary>
			/// The multiplication operator
			/// </summary>
			MUL,
			/// <summary>
			/// The division operator
			/// </summary>
			DIV,
			/// <summary>
			/// The floor division operator
			/// </summary>
			FLOORDIV,
			/// <summary>
			/// The modulo operator
			/// </summary>
			MOD,
			/// <summary>
			/// The power operator
			/// </summary>
			POW,
			/// <summary>
			/// The bitwise and operator
			/// </summary>
			BITAND,
			/// <summary>
			/// The bitwise or operator
			/// </summary>
			BITOR,
			/// <summary>
			/// The xor operator
			/// </summary>
			BITXOR,
			/// <summary>
			/// The logical and operator
			/// </summary>
			AND,
			/// <summary>
			/// The logical or operator
			/// </summary>
			OR,
			/// <summary>
			/// The bit left shift operator
			/// </summary>
			SHL,
			/// <summary>
			/// The bit right shift operator
			/// </summary>
			SHR,
			/// <summary>
			/// The in operator
			/// </summary>
			IN,
			/// <summary>
			/// The not in operator
			/// </summary>
			NOTIN,
			/// <summary>
			/// The equals operator
			/// </summary>
			EQ,
			/// <summary>
			/// The not equals operator
			/// </summary>
			NE,
			/// <summary>
			/// The less than operator
			/// </summary>
			LT,
			/// <summary>
			/// The less than or equals operator
			/// </summary>
			LE,
			/// <summary>
			/// The greater than operator
			/// </summary>
			GT,
			/// <summary>
			/// The greater than or equals operator
			/// </summary>
			GE,
		}

		public enum Exc {
			/// <summary>
			/// BaseException
			/// </summary>
			BASEEXCEPTION,
			/// <summary>
			/// WingsTimeoutError
			/// </summary>
			WINGSTIMEOUTERROR,
			/// <summary>
			/// SystemExit
			/// </summary>
			SYSTEMEXIT,
			/// <summary>
			/// Exception
			/// </summary>
			EXCEPTION,
			/// <summary>
			/// StopIteration
			/// </summary>
			STOPITERATION,
			/// <summary>
			/// ArithmeticError
			/// </summary>
			ARITHMETICERROR,
			/// <summary>
			/// OverflowError
			/// </summary>
			OVERFLOWERROR,
			/// <summary>
			/// ZeroDivisionError
			/// </summary>
			ZERODIVISIONERROR,
			/// <summary>
			/// AttributeError
			/// </summary>
			ATTRIBUTEERROR,
			/// <summary>
			/// ImportError
			/// </summary>
			IMPORTERROR,
			/// <summary>
			/// LookupError
			/// </summary>
			LOOKUPERROR,
			/// <summary>
			/// IndexError
			/// </summary>
			INDEXERROR,
			/// <summary>
			/// KeyError
			/// </summary>
			KEYERROR,
			/// <summary>
			/// MemoryError
			/// </summary>
			MEMORYERROR,
			/// <summary>
			/// NameError
			/// </summary>
			NAMEERROR,
			/// <summary>
			/// OSError
			/// </summary>
			OSERROR,
			/// <summary>
			/// IsADirectoryError
			/// </summary>
			ISADIRECTORYERROR,
			/// <summary>
			/// RuntimeError
			/// </summary>
			RUNTIMEERROR,
			/// <summary>
			/// NotImplementedError
			/// </summary>
			NOTIMPLEMENTEDERROR,
			/// <summary>
			/// RecursionError
			/// </summary>
			RECURSIONERROR,
			/// <summary>
			/// SyntaxError
			/// </summary>
			SYNTAXERROR,
			/// <summary>
			/// TypeError
			/// </summary>
			TYPEERROR,
			/// <summary>
			/// ValueError
			/// </summary>
			VALUEERROR,
		}

		/// <summary>
		/// Create an instance of an interpreter.
		/// </summary>
		/// <param name="config">
		/// The configuration to use, or null to use the default configuration.
		/// </param>
		/// <returns>
		/// A newly created context.
		/// </returns>
		public static Context CreateContext(Config? config = default) {
			unsafe {
				Context r;
				var _config = config is null ? new() : new Wg_ConfigNative(config.Value);
				r = Wg_CreateContext(config is null ? IntPtr.Zero : new IntPtr(&_config));
				if (config != null) {
					_config.Free(config.Value);
				}
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Context Wg_CreateContext(IntPtr config);

		/// <summary>
		/// Free a context created with CreateContext().
		/// </summary>
		/// <param name="context">
		/// The context to free.
		/// </param>
		public static void DestroyContext(Context context) {
			unsafe {
				Wg_DestroyContext(context);
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe void Wg_DestroyContext(Context context);

		/// <summary>
		/// Get the default configuration.
		/// </summary>
		/// <param name="config">
		/// The returned default configuration.
		/// </param>
		/// <see>
		/// CreateContext
		/// </see>
		public static void DefaultConfig(out Config config) {
			unsafe {
				Wg_DefaultConfig(out var _config);
				config = MakeWg_Config(_config);
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe void Wg_DefaultConfig(out Wg_ConfigNative config);

		/// <summary>
		/// Execute a script.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <param name="script">
		/// The script to execute.
		/// </param>
		/// <param name="prettyName">
		/// The name to run the script under, or null to use a default name.
		/// </param>
		/// <returns>
		/// A boolean indicating whether the script compiled and executed successfully.
		/// </returns>
		/// <see>
		/// GetException
		/// GetErrorMessage
		/// </see>
		public static bool Execute(Context context, string script, string? prettyName = default) {
			unsafe {
				bool r;
				fixed (byte* _script = script is null ? null : Encoding.ASCII.GetBytes(script + '\0')) {
					fixed (byte* _prettyName = prettyName is null ? null : Encoding.ASCII.GetBytes(prettyName + '\0')) {
						r = Wg_Execute(context, (IntPtr)_script, (IntPtr)_prettyName) != 0;
					}
				}
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe byte Wg_Execute(Context context, IntPtr script, IntPtr prettyName);

		/// <summary>
		/// Execute a script containing an expression.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <param name="script">
		/// The expression to execute.
		/// </param>
		/// <param name="prettyName">
		/// The name to run the script under, or null to use a default name.
		/// </param>
		/// <returns>
		/// The result of executing the expression, or null on failure.
		/// </returns>
		/// <see>
		/// GetException
		/// GetErrorMessage
		/// </see>
		public static Obj ExecuteExpression(Context context, string script, string? prettyName = default) {
			unsafe {
				Obj r;
				fixed (byte* _script = script is null ? null : Encoding.ASCII.GetBytes(script + '\0')) {
					fixed (byte* _prettyName = prettyName is null ? null : Encoding.ASCII.GetBytes(prettyName + '\0')) {
						r = Wg_ExecuteExpression(context, (IntPtr)_script, (IntPtr)_prettyName);
					}
				}
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_ExecuteExpression(Context context, IntPtr script, IntPtr prettyName);

		/// <summary>
		/// Compile a script into a function object.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <param name="script">
		/// The script to compile.
		/// </param>
		/// <param name="prettyName">
		/// The name to run the script under, or null to use a default name.
		/// </param>
		/// <returns>
		/// A function object, or null on failure.
		/// </returns>
		/// <see>
		/// GetException
		/// GetErrorMessage
		/// Call
		/// </see>
		public static Obj Compile(Context context, string script, string? prettyName = default) {
			unsafe {
				Obj r;
				fixed (byte* _script = script is null ? null : Encoding.ASCII.GetBytes(script + '\0')) {
					fixed (byte* _prettyName = prettyName is null ? null : Encoding.ASCII.GetBytes(prettyName + '\0')) {
						r = Wg_Compile(context, (IntPtr)_script, (IntPtr)_prettyName);
					}
				}
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_Compile(Context context, IntPtr script, IntPtr prettyName);

		/// <summary>
		/// Compile an expression into a function object.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <param name="script">
		/// The expression to compile.
		/// </param>
		/// <param name="prettyName">
		/// The name to run the script under, or null to use a default name.
		/// </param>
		/// <returns>
		/// A function object, or null on failure.
		/// </returns>
		/// <see>
		/// GetException
		/// GetErrorMessage
		/// Call
		/// </see>
		public static Obj CompileExpression(Context context, string script, string? prettyName = default) {
			unsafe {
				Obj r;
				fixed (byte* _script = script is null ? null : Encoding.ASCII.GetBytes(script + '\0')) {
					fixed (byte* _prettyName = prettyName is null ? null : Encoding.ASCII.GetBytes(prettyName + '\0')) {
						r = Wg_CompileExpression(context, (IntPtr)_script, (IntPtr)_prettyName);
					}
				}
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_CompileExpression(Context context, IntPtr script, IntPtr prettyName);

		/// <summary>
		/// Set a callback for programmer errors.
		/// </summary>
		/// <param name="callback">
		/// The callback to use, or null to default to abort().
		/// </param>
		public static void SetErrorCallback(ErrorCallback callback) {
			unsafe {
				Wg_SetErrorCallback(callback);
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe void Wg_SetErrorCallback(ErrorCallback callback);

		/// <summary>
		/// Get the current exception message.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <returns>
		/// The current error string.
		/// </returns>
		/// <see>
		/// GetException
		/// RaiseException
		/// RaiseExceptionClass
		/// RaiseExceptionObject
		/// ReraiseExceptionObject
		/// </see>
		public static string GetErrorMessage(Context context) {
			unsafe {
				string r;
				r = Marshal.PtrToStringAnsi(Wg_GetErrorMessage(context))!;
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe IntPtr Wg_GetErrorMessage(Context context);

		/// <summary>
		/// Get the current exception object.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <returns>
		/// The current exception object, or null if there is no exception.
		/// </returns>
		/// <see>
		/// GetErrorMessage
		/// RaiseException
		/// RaiseExceptionClass
		/// RaiseExceptionObject
		/// ReraiseExceptionObject
		/// </see>
		public static Obj GetException(Context context) {
			unsafe {
				Obj r;
				r = Wg_GetException(context);
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_GetException(Context context);

		/// <summary>
		/// Set a timeout before a WingTimeoutError is raised.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <param name="milliseconds">
		/// The timeout in milliseconds.
		/// </param>
		/// <see>
		/// ClearTimeout
		/// CheckTimeout
		/// </see>
		public static void SetTimeout(Context context, int milliseconds) {
			unsafe {
				Wg_SetTimeout(context, milliseconds);
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe void Wg_SetTimeout(Context context, int milliseconds);

		/// <summary>
		/// Pop the previous timeout.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <see>
		/// SetTimeout
		/// CheckTimeout
		/// </see>
		public static void ClearTimeout(Context context) {
			unsafe {
				Wg_ClearTimeout(context);
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe void Wg_ClearTimeout(Context context);

		/// <summary>
		/// Check if any timeout has occurred.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <returns>
		/// True if any timeout has occurred, otherwise false.
		/// </returns>
		/// <see>
		/// SetTimeout
		/// ClearTimeout
		/// </see>
		public static bool CheckTimeout(Context context) {
			unsafe {
				bool r;
				r = Wg_CheckTimeout(context) != 0;
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe byte Wg_CheckTimeout(Context context);

		/// <summary>
		/// Create and raise an exception.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <param name="type">
		/// The exception type.
		/// </param>
		/// <param name="message">
		/// The error message, or null for an empty string.
		/// </param>
		/// <see>
		/// RaiseExceptionClass
		/// RaiseExceptionObject
		/// ReraiseExceptionObject
		/// </see>
		public static void RaiseException(Context context, Exc type, string? message = default) {
			unsafe {
				fixed (byte* _message = message is null ? null : Encoding.ASCII.GetBytes(message + '\0')) {
					Wg_RaiseException(context, type, (IntPtr)_message);
				}
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe void Wg_RaiseException(Context context, Exc type, IntPtr message);

		/// <summary>
		/// Create and raise an exception using a class object.
		/// </summary>
		/// <param name="klass">
		/// The exception class.
		/// </param>
		/// <param name="message">
		/// The error message, or null for an empty string.
		/// </param>
		/// <see>
		/// RaiseException
		/// RaiseExceptionObject
		/// ReraiseExceptionObject
		/// </see>
		public static void RaiseExceptionClass(Obj klass, string? message = default) {
			unsafe {
				fixed (byte* _message = message is null ? null : Encoding.ASCII.GetBytes(message + '\0')) {
					Wg_RaiseExceptionClass(klass, (IntPtr)_message);
				}
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe void Wg_RaiseExceptionClass(Obj klass, IntPtr message);

		/// <summary>
		/// Raise an existing exception object.
		/// </summary>
		/// <param name="obj">
		/// The exception object to raise. The type must be a subclass of BaseException.
		/// </param>
		/// <see>
		/// RaiseException
		/// RaiseExceptionClass
		/// ReraiseExceptionObject
		/// </see>
		public static void RaiseExceptionObject(Obj obj) {
			unsafe {
				Wg_RaiseExceptionObject(obj);
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe void Wg_RaiseExceptionObject(Obj obj);

		/// <summary>
		/// Raise an existing exception object without affecting the stack trace.
		/// </summary>
		/// <param name="obj">
		/// The exception object to raise. The type must be a subclass of BaseException.
		/// </param>
		/// <see>
		/// RaiseException
		/// RaiseExceptionClass
		/// RaiseExceptionObject
		/// </see>
		public static void ReraiseExceptionObject(Obj obj) {
			unsafe {
				Wg_ReraiseExceptionObject(obj);
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe void Wg_ReraiseExceptionObject(Obj obj);

		/// <summary>
		/// Raise a TypeError with a formatted message.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <param name="given">
		/// The number of arguments given.
		/// </param>
		/// <param name="expected">
		/// The number of arguments expected, or -1 if this is not a fixed number.
		/// </param>
		/// <see>
		/// RaiseException
		/// RaiseExceptionClass
		/// RaiseExceptionObject
		/// </see>
		public static void RaiseArgumentCountError(Context context, int given, int expected) {
			unsafe {
				Wg_RaiseArgumentCountError(context, given, expected);
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe void Wg_RaiseArgumentCountError(Context context, int given, int expected);

		/// <summary>
		/// Raise a TypeError with a formatted message.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <param name="index">
		/// The parameter index of the invalid argument.
		/// </param>
		/// <param name="expected">
		/// A string describing the expected type.
		/// </param>
		/// <see>
		/// RaiseException
		/// RaiseExceptionClass
		/// RaiseExceptionObject
		/// </see>
		public static void RaiseArgumentTypeError(Context context, int index, string expected) {
			unsafe {
				fixed (byte* _expected = expected is null ? null : Encoding.ASCII.GetBytes(expected + '\0')) {
					Wg_RaiseArgumentTypeError(context, index, (IntPtr)_expected);
				}
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe void Wg_RaiseArgumentTypeError(Context context, int index, IntPtr expected);

		/// <summary>
		/// Raise a AttributeError with a formatted message.
		/// </summary>
		/// <param name="obj">
		/// The object missing the attribute.
		/// </param>
		/// <param name="attribute">
		/// The missing attribute.
		/// </param>
		/// <see>
		/// RaiseException
		/// RaiseExceptionClass
		/// RaiseExceptionObject
		/// </see>
		public static void RaiseAttributeError(Obj obj, string attribute) {
			unsafe {
				fixed (byte* _attribute = attribute is null ? null : Encoding.ASCII.GetBytes(attribute + '\0')) {
					Wg_RaiseAttributeError(obj, (IntPtr)_attribute);
				}
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe void Wg_RaiseAttributeError(Obj obj, IntPtr attribute);

		/// <summary>
		/// Raise a KeyError with a formatted message.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <param name="key">
		/// The key that caused the KeyError, or null to leave unspecified.
		/// </param>
		/// <see>
		/// RaiseException
		/// RaiseExceptionClass
		/// RaiseExceptionObject
		/// </see>
		public static void RaiseKeyError(Context context, Obj key = default) {
			unsafe {
				Wg_RaiseKeyError(context, key);
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe void Wg_RaiseKeyError(Context context, Obj key);

		/// <summary>
		/// Raise a NameError with a formatted message.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <param name="name">
		/// The name that was not found.
		/// </param>
		/// <see>
		/// RaiseException
		/// RaiseExceptionClass
		/// RaiseExceptionObject
		/// </see>
		public static void RaiseNameError(Context context, string name) {
			unsafe {
				fixed (byte* _name = name is null ? null : Encoding.ASCII.GetBytes(name + '\0')) {
					Wg_RaiseNameError(context, (IntPtr)_name);
				}
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe void Wg_RaiseNameError(Context context, IntPtr name);

		/// <summary>
		/// Clear the current exception.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		public static void ClearException(Context context) {
			unsafe {
				Wg_ClearException(context);
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe void Wg_ClearException(Context context);

		/// <summary>
		/// Get the context associated with an object.
		/// </summary>
		/// <param name="obj">
		/// The object.
		/// </param>
		/// <returns>
		/// The associated context.
		/// </returns>
		public static Context GetContextFromObject(Obj obj) {
			unsafe {
				Context r;
				r = Wg_GetContextFromObject(obj);
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Context Wg_GetContextFromObject(Obj obj);

		/// <summary>
		/// Check if an object's class derives from any of the specified classes.
		/// </summary>
		/// <param name="instance">
		/// The object to be checked.
		/// </param>
		/// <param name="types">
		/// An array of class objects to be checked against.
		/// This can be null if typesLen is 0.
		/// </param>
		/// <param name="typesLen">
		/// The length of the types array.
		/// </param>
		/// <returns>
		/// The first subclass matched, or null if the object's
		/// class does not derive from any of the specified classes.
		/// </returns>
		public static Obj IsInstance(Obj instance, Obj[] types, int typesLen) {
			unsafe {
				Obj r;
				fixed (Obj* _types = types) {
					r = Wg_IsInstance(instance, (IntPtr)_types, typesLen);
				}
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_IsInstance(Obj instance, IntPtr types, int typesLen);

		/// <summary>
		/// Get a global variable in the current module namespace.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <param name="name">
		/// The name of the global variable.
		/// </param>
		/// <returns>
		/// The value of the global variable, or null if it does not exist.
		/// </returns>
		/// <see>
		/// SetGlobal
		/// </see>
		public static Obj GetGlobal(Context context, string name) {
			unsafe {
				Obj r;
				fixed (byte* _name = name is null ? null : Encoding.ASCII.GetBytes(name + '\0')) {
					r = Wg_GetGlobal(context, (IntPtr)_name);
				}
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_GetGlobal(Context context, IntPtr name);

		/// <summary>
		/// Set a global variable in the current module namespace.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <param name="name">
		/// The name of the global variable.
		/// </param>
		/// <param name="value">
		/// The value to set.
		/// </param>
		/// <see>
		/// GetGlobal
		/// </see>
		public static void SetGlobal(Context context, string name, Obj value) {
			unsafe {
				fixed (byte* _name = name is null ? null : Encoding.ASCII.GetBytes(name + '\0')) {
					Wg_SetGlobal(context, (IntPtr)_name, value);
				}
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe void Wg_SetGlobal(Context context, IntPtr name, Obj value);

		/// <summary>
		/// Print a message.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <param name="message">
		/// A byte array containing the message.
		/// This can null if len is 0.
		/// </param>
		/// <param name="len">
		/// The length of the message byte array.
		/// </param>
		/// <see>
		/// PrintString
		/// Config
		/// </see>
		public static void Print(Context context, string message, int len) {
			unsafe {
				fixed (byte* _message = message is null ? null : Encoding.ASCII.GetBytes(message + '\0')) {
					Wg_Print(context, (IntPtr)_message, len);
				}
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe void Wg_Print(Context context, IntPtr message, int len);

		/// <summary>
		/// Print a string message.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <param name="message">
		/// A null terminated message string.
		/// </param>
		/// <see>
		/// Print
		/// Config
		/// </see>
		public static void PrintString(Context context, string message) {
			unsafe {
				fixed (byte* _message = message is null ? null : Encoding.ASCII.GetBytes(message + '\0')) {
					Wg_PrintString(context, (IntPtr)_message);
				}
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe void Wg_PrintString(Context context, IntPtr message);

		/// <summary>
		/// Force run the garbage collector and free all unreachable objects.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		public static void CollectGarbage(Context context) {
			unsafe {
				Wg_CollectGarbage(context);
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe void Wg_CollectGarbage(Context context);

		/// <summary>
		/// Increment the reference count of an object.
		/// A positive reference count prevents the object from being garbage collected.
		/// </summary>
		/// <param name="obj">
		/// The object whose reference count is to be incremented.
		/// </param>
		/// <see>
		/// DecRef
		/// </see>
		public static void IncRef(Obj obj) {
			unsafe {
				Wg_IncRef(obj);
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe void Wg_IncRef(Obj obj);

		/// <summary>
		/// Decrement the reference count of an object.
		/// A positive reference count prevents the object from being garbage collected.
		/// </summary>
		/// <param name="obj">
		/// The object whose reference count is to be decremented.
		/// </param>
		/// <see>
		/// IncRef
		/// </see>
		public static void DecRef(Obj obj) {
			unsafe {
				Wg_DecRef(obj);
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe void Wg_DecRef(Obj obj);

		/// <summary>
		/// Get the None singleton value.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <returns>
		/// The None singleton object.
		/// </returns>
		public static Obj None(Context context) {
			unsafe {
				Obj r;
				r = Wg_None(context);
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_None(Context context);

		/// <summary>
		/// Instantiate a boolean object.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <param name="value">
		/// The value of the object.
		/// </param>
		/// <returns>
		/// The instantiated object.
		/// </returns>
		public static Obj NewBool(Context context, bool value = default) {
			unsafe {
				Obj r;
				r = Wg_NewBool(context, (byte)(value ? 1 : 0));
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_NewBool(Context context, byte value);

		/// <summary>
		/// Instantiate an integer object.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <param name="value">
		/// The value of the object.
		/// </param>
		/// <returns>
		/// The instantiated object, or null on failure.
		/// </returns>
		/// <see>
		/// GetException
		/// GetErrorMessage
		/// </see>
		public static Obj NewInt(Context context, long value = default) {
			unsafe {
				Obj r;
				r = Wg_NewInt(context, value);
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_NewInt(Context context, long value);

		/// <summary>
		/// Instantiate a float object.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <param name="value">
		/// The value of the object.
		/// </param>
		/// <returns>
		/// The instantiated object, or null on failure.
		/// </returns>
		/// <see>
		/// GetException
		/// GetErrorMessage
		/// </see>
		public static Obj NewFloat(Context context, float value = default) {
			unsafe {
				Obj r;
				r = Wg_NewFloat(context, value);
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_NewFloat(Context context, float value);

		/// <summary>
		/// Instantiate a string object.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <param name="value">
		/// A null terminated string, or null for an empty string.
		/// </param>
		/// <returns>
		/// The instantiated object, or null on failure.
		/// </returns>
		/// <see>
		/// GetException
		/// GetErrorMessage
		/// </see>
		public static Obj NewString(Context context, string? value = default) {
			unsafe {
				Obj r;
				fixed (byte* _value = value is null ? null : Encoding.ASCII.GetBytes(value + '\0')) {
					r = Wg_NewString(context, (IntPtr)_value);
				}
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_NewString(Context context, IntPtr value);

		/// <summary>
		/// Instantiate a string object from a buffer.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <param name="buffer">
		/// The buffer.
		/// </param>
		/// <param name="len">
		/// The length of the buffer.
		/// </param>
		/// <returns>
		/// The instantiated object, or null on failure.
		/// </returns>
		/// <see>
		/// GetException
		/// GetErrorMessage
		/// </see>
		public static Obj NewStringBuffer(Context context, string buffer, int len) {
			unsafe {
				Obj r;
				fixed (byte* _buffer = buffer is null ? null : Encoding.ASCII.GetBytes(buffer + '\0')) {
					r = Wg_NewStringBuffer(context, (IntPtr)_buffer, len);
				}
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_NewStringBuffer(Context context, IntPtr buffer, int len);

		/// <summary>
		/// Instantiate a tuple object.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <param name="argv">
		/// An array of objects to initialise the tuple with.
		/// This can be null if argc is 0.
		/// </param>
		/// <param name="argc">
		/// The length of the argv array.
		/// </param>
		/// <returns>
		/// The instantiated object, or null on failure.
		/// </returns>
		/// <see>
		/// GetException
		/// GetErrorMessage
		/// </see>
		public static Obj NewTuple(Context context, Obj[] argv, int argc) {
			unsafe {
				Obj r;
				fixed (Obj* _argv = argv) {
					r = Wg_NewTuple(context, (IntPtr)_argv, argc);
				}
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_NewTuple(Context context, IntPtr argv, int argc);

		/// <summary>
		/// Instantiate a list object.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <param name="argv">
		/// An array of objects to initialise the list with.
		/// This can be null if argc is 0.
		/// </param>
		/// <param name="argc">
		/// The length of the argv array.
		/// </param>
		/// <returns>
		/// The instantiated object, or null on failure.
		/// </returns>
		/// <see>
		/// GetException
		/// GetErrorMessage
		/// </see>
		public static Obj NewList(Context context, Obj[]? argv = default, int argc = default) {
			unsafe {
				Obj r;
				fixed (Obj* _argv = argv) {
					r = Wg_NewList(context, (IntPtr)_argv, argc);
				}
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_NewList(Context context, IntPtr argv, int argc);

		/// <summary>
		/// Instantiate a dictionary object.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <param name="keys">
		/// An array of keys to initialise the dictionary with.
		/// This can be null if argc is 0.
		/// </param>
		/// <param name="values">
		/// An array of values to initialise the dictionary with.
		/// This can be null if argc is 0.
		/// </param>
		/// <param name="len">
		/// The length of the keys and values arrays.
		/// </param>
		/// <returns>
		/// The instantiated object, or null on failure.
		/// </returns>
		/// <see>
		/// GetException
		/// GetErrorMessage
		/// </see>
		public static Obj NewDictionary(Context context, Obj[]? keys = default, Obj[]? values = default, int len = default) {
			unsafe {
				Obj r;
				fixed (Obj* _keys = keys) {
					fixed (Obj* _values = values) {
						r = Wg_NewDictionary(context, (IntPtr)_keys, (IntPtr)_values, len);
					}
				}
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_NewDictionary(Context context, IntPtr keys, IntPtr values, int len);

		/// <summary>
		/// Instantiate a set object.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <param name="argv">
		/// An array of objects to initialise the set with.
		/// This can be null if argc is 0.
		/// </param>
		/// <param name="argc">
		/// The length of the argv array.
		/// </param>
		/// <returns>
		/// The instantiated object, or null on failure.
		/// </returns>
		/// <see>
		/// GetException
		/// GetErrorMessage
		/// </see>
		public static Obj NewSet(Context context, Obj[]? argv = default, int argc = default) {
			unsafe {
				Obj r;
				fixed (Obj* _argv = argv) {
					r = Wg_NewSet(context, (IntPtr)_argv, argc);
				}
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_NewSet(Context context, IntPtr argv, int argc);

		/// <summary>
		/// Instantiate a function object.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <param name="fptr">
		/// The native function to be bound.
		/// </param>
		/// <param name="userdata">
		/// The userdata to pass to the function when it is called.
		/// </param>
		/// <param name="prettyName">
		/// The name of the function, or null to use a default name.
		/// </param>
		/// <returns>
		/// The instantiated object, or null on failure.
		/// </returns>
		/// <see>
		/// GetException
		/// GetErrorMessage
		/// BindMethod
		/// </see>
		public static Obj NewFunction(Context context, Function fptr, IntPtr userdata, string? prettyName = default) {
			unsafe {
				Obj r;
				fixed (byte* _prettyName = prettyName is null ? null : Encoding.ASCII.GetBytes(prettyName + '\0')) {
					r = Wg_NewFunction(context, fptr, userdata, (IntPtr)_prettyName);
				}
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_NewFunction(Context context, Function fptr, IntPtr userdata, IntPtr prettyName);

		/// <summary>
		/// Instantiate a function object and bind it to a class.
		/// </summary>
		/// <param name="klass">
		/// The class to bind the method to.
		/// </param>
		/// <param name="name">
		/// The name of the method.
		/// </param>
		/// <param name="fptr">
		/// The native function to be bound.
		/// </param>
		/// <param name="userdata">
		/// The userdata to pass to the function when it is called.
		/// </param>
		/// <returns>
		/// The instantiated object, or null on failure.
		/// </returns>
		/// <see>
		/// GetException
		/// GetErrorMessage
		/// BindMethod
		/// </see>
		/// <remarks>
		/// Existing instances of the class will gain the new method.
		/// </remarks>
		public static Obj BindMethod(Obj klass, string name, Function fptr, IntPtr userdata) {
			unsafe {
				Obj r;
				fixed (byte* _name = name is null ? null : Encoding.ASCII.GetBytes(name + '\0')) {
					r = Wg_BindMethod(klass, (IntPtr)_name, fptr, userdata);
				}
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_BindMethod(Obj klass, IntPtr name, Function fptr, IntPtr userdata);

		/// <summary>
		/// Instantiate a class object.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <param name="name">
		/// The name of the class.
		/// </param>
		/// <param name="bases">
		/// An array of class objects to be used as a base.
		/// This can be null if basesLen is 0.
		/// </param>
		/// <param name="basesLen">
		/// The length of the bases array.
		/// </param>
		/// <returns>
		/// The instantiated class object, or null on failure.
		/// </returns>
		/// <see>
		/// GetException
		/// GetErrorMessage
		/// </see>
		public static Obj NewClass(Context context, string name, Obj[] bases, int basesLen) {
			unsafe {
				Obj r;
				fixed (byte* _name = name is null ? null : Encoding.ASCII.GetBytes(name + '\0')) {
					fixed (Obj* _bases = bases) {
						r = Wg_NewClass(context, (IntPtr)_name, (IntPtr)_bases, basesLen);
					}
				}
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_NewClass(Context context, IntPtr name, IntPtr bases, int basesLen);

		/// <summary>
		/// Check if an object is None.
		/// </summary>
		/// <param name="obj">
		/// The object to inspect.
		/// </param>
		/// <returns>
		/// True if the object is None, otherwise false.
		/// </returns>
		public static bool IsNone(Obj obj) {
			unsafe {
				bool r;
				r = Wg_IsNone(obj) != 0;
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe byte Wg_IsNone(Obj obj);

		/// <summary>
		/// Check if an object is a boolean.
		/// </summary>
		/// <param name="obj">
		/// The object to inspect.
		/// </param>
		/// <returns>
		/// True if the object is a boolean, otherwise false.
		/// </returns>
		public static bool IsBool(Obj obj) {
			unsafe {
				bool r;
				r = Wg_IsBool(obj) != 0;
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe byte Wg_IsBool(Obj obj);

		/// <summary>
		/// Check if an object is an integer.
		/// </summary>
		/// <param name="obj">
		/// The object to inspect.
		/// </param>
		/// <returns>
		/// True if the object is an integer, otherwise false.
		/// </returns>
		public static bool IsInt(Obj obj) {
			unsafe {
				bool r;
				r = Wg_IsInt(obj) != 0;
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe byte Wg_IsInt(Obj obj);

		/// <summary>
		/// Check if an object is a float.
		/// </summary>
		/// <param name="obj">
		/// The object to inspect.
		/// </param>
		/// <returns>
		/// True if the object is an integer or float, otherwise false.
		/// </returns>
		public static bool IsIntOrFloat(Obj obj) {
			unsafe {
				bool r;
				r = Wg_IsIntOrFloat(obj) != 0;
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe byte Wg_IsIntOrFloat(Obj obj);

		/// <summary>
		/// Check if an object is a string.
		/// </summary>
		/// <param name="obj">
		/// The object to inspect.
		/// </param>
		/// <returns>
		/// True if the object is a string, otherwise false.
		/// </returns>
		public static bool IsString(Obj obj) {
			unsafe {
				bool r;
				r = Wg_IsString(obj) != 0;
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe byte Wg_IsString(Obj obj);

		/// <summary>
		/// Check if an object is a tuple.
		/// </summary>
		/// <param name="obj">
		/// The object to inspect.
		/// </param>
		/// <returns>
		/// True if the object is a tuple, otherwise false.
		/// </returns>
		public static bool IsTuple(Obj obj) {
			unsafe {
				bool r;
				r = Wg_IsTuple(obj) != 0;
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe byte Wg_IsTuple(Obj obj);

		/// <summary>
		/// Check if an object is a list.
		/// </summary>
		/// <param name="obj">
		/// The object to inspect.
		/// </param>
		/// <returns>
		/// True if the object is a list, otherwise false.
		/// </returns>
		public static bool IsList(Obj obj) {
			unsafe {
				bool r;
				r = Wg_IsList(obj) != 0;
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe byte Wg_IsList(Obj obj);

		/// <summary>
		/// Check if an object is a dictionary.
		/// </summary>
		/// <param name="obj">
		/// The object to inspect.
		/// </param>
		/// <returns>
		/// True if the object is a dictionary, otherwise false.
		/// </returns>
		public static bool IsDictionary(Obj obj) {
			unsafe {
				bool r;
				r = Wg_IsDictionary(obj) != 0;
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe byte Wg_IsDictionary(Obj obj);

		/// <summary>
		/// Check if an object is a set.
		/// </summary>
		/// <param name="obj">
		/// The object to inspect.
		/// </param>
		/// <returns>
		/// True if the object is a set, otherwise false.
		/// </returns>
		public static bool IsSet(Obj obj) {
			unsafe {
				bool r;
				r = Wg_IsSet(obj) != 0;
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe byte Wg_IsSet(Obj obj);

		/// <summary>
		/// Check if an object is a function.
		/// </summary>
		/// <param name="obj">
		/// The object to inspect.
		/// </param>
		/// <returns>
		/// True if the object is a function, otherwise false.
		/// </returns>
		public static bool IsFunction(Obj obj) {
			unsafe {
				bool r;
				r = Wg_IsFunction(obj) != 0;
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe byte Wg_IsFunction(Obj obj);

		/// <summary>
		/// Check if an object is a class.
		/// </summary>
		/// <param name="obj">
		/// The object to inspect.
		/// </param>
		/// <returns>
		/// True if the object is a class, otherwise false.
		/// </returns>
		public static bool IsClass(Obj obj) {
			unsafe {
				bool r;
				r = Wg_IsClass(obj) != 0;
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe byte Wg_IsClass(Obj obj);

		/// <summary>
		/// Get the value from a boolean object.
		/// </summary>
		/// <param name="obj">
		/// The object to get the value from.
		/// </param>
		/// <returns>
		/// The boolean value of the object.
		/// </returns>
		public static bool GetBool(Obj obj) {
			unsafe {
				bool r;
				r = Wg_GetBool(obj) != 0;
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe byte Wg_GetBool(Obj obj);

		/// <summary>
		/// Get the value from an integer object.
		/// </summary>
		/// <param name="obj">
		/// The object to get the value from.
		/// </param>
		/// <returns>
		/// The integer value of the object.
		/// </returns>
		public static long GetInt(Obj obj) {
			unsafe {
				long r;
				r = Wg_GetInt(obj);
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe long Wg_GetInt(Obj obj);

		/// <summary>
		/// Get the float value from an integer or float object.
		/// </summary>
		/// <param name="obj">
		/// The object to get the value from.
		/// </param>
		/// <returns>
		/// The float value of the object.
		/// </returns>
		public static float GetFloat(Obj obj) {
			unsafe {
				float r;
				r = Wg_GetFloat(obj);
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe float Wg_GetFloat(Obj obj);

		/// <summary>
		/// Get the value from a string object.
		/// </summary>
		/// <param name="obj">
		/// The object to get the value from.
		/// </param>
		/// <param name="len">
		/// The length of the string. This parameter may be null.
		/// </param>
		/// <returns>
		/// The string value of the object.
		/// </returns>
		/// <remarks>
		/// The string is always null terminated. If null bytes are
		/// expected to appear in the middle of the string, the length parameter
		/// can be used to get the true length of the string.
		/// </remarks>
		public static string GetString(Obj obj, out int len) {
			unsafe {
				string r;
				r = Marshal.PtrToStringAnsi(Wg_GetString(obj, out len))!;
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe IntPtr Wg_GetString(Obj obj, out int len);

		/// <summary>
		/// Set the userdata for an object.
		/// </summary>
		/// <param name="obj">
		/// The object to set the userdata for.
		/// </param>
		/// <param name="userdata">
		/// The userdata to set.
		/// </param>
		/// <see>
		/// TryGetUserdata
		/// </see>
		/// <remarks>
		/// Only call this function on objects instantiated from user-created
		/// classes created with NewClass(). Do not set the userdata for builtin types.
		/// </remarks>
		public static void SetUserdata(Obj obj, IntPtr userdata) {
			unsafe {
				Wg_SetUserdata(obj, userdata);
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe void Wg_SetUserdata(Obj obj, IntPtr userdata);

		/// <summary>
		/// Get the userdata from an object if it is of the expected type.
		/// </summary>
		/// <param name="obj">
		/// The object to get the value from.
		/// </param>
		/// <param name="type">
		/// The type to match.
		/// </param>
		/// <param name="userdata">
		/// The userdata. This parameter may be null.
		/// </param>
		/// <returns>
		/// A boolean indicating whether obj matches type.
		/// </returns>
		/// <see>
		/// SetUserdata
		/// NewClass
		/// </see>
		public static bool TryGetUserdata(Obj obj, string type, out IntPtr userdata) {
			unsafe {
				bool r;
				fixed (byte* _type = type is null ? null : Encoding.ASCII.GetBytes(type + '\0')) {
					r = Wg_TryGetUserdata(obj, (IntPtr)_type, out userdata) != 0;
				}
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe byte Wg_TryGetUserdata(Obj obj, IntPtr type, out IntPtr userdata);

		/// <summary>
		/// Register a finalizer to run when an object is garbage collected.
		/// </summary>
		/// <param name="obj">
		/// The object to register the finalizer for.
		/// </param>
		/// <param name="finalizer">
		/// The finalizer function.
		/// </param>
		/// <param name="userdata">
		/// The userdata to pass to the finalizer function.
		/// </param>
		/// <remarks>
		/// Do not instantiate any objects in the finalizer.
		/// </remarks>
		public static void RegisterFinalizer(Obj obj, Finalizer finalizer, IntPtr userdata = default) {
			unsafe {
				Wg_RegisterFinalizer(obj, finalizer, userdata);
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe void Wg_RegisterFinalizer(Obj obj, Finalizer finalizer, IntPtr userdata);

		/// <summary>
		/// Check if an object has an attribute.
		/// </summary>
		/// <param name="obj">
		/// The object to check.
		/// </param>
		/// <param name="attribute">
		/// The attribute to check.
		/// </param>
		/// <returns>
		/// A boolean indicating whether the object has the attribute.
		/// </returns>
		/// <see>
		/// GetAttribute
		/// GetAttributeFromBase
		/// GetAttributeNoExcept
		/// SetAttribute
		/// </see>
		public static bool HasAttribute(Obj obj, string attribute) {
			unsafe {
				bool r;
				fixed (byte* _attribute = attribute is null ? null : Encoding.ASCII.GetBytes(attribute + '\0')) {
					r = Wg_HasAttribute(obj, (IntPtr)_attribute) != 0;
				}
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe byte Wg_HasAttribute(Obj obj, IntPtr attribute);

		/// <summary>
		/// Get an attribute of an object.
		/// If the attribute does not exist, an AttributeError is raised.
		/// If the attribute is an unbound method object,
		/// a new method object is allocated with obj bound.
		/// If this allocation fails, a MemoryError is raised.
		/// </summary>
		/// <param name="obj">
		/// The object to get the attribute from.
		/// </param>
		/// <param name="attribute">
		/// The attribute to get.
		/// </param>
		/// <returns>
		/// The attribute value, or null if the attribute
		/// does not exist or there is an error.
		/// </returns>
		/// <see>
		/// HasAttribute
		/// GetAttributeFromBase
		/// GetAttributeNoExcept
		/// SetAttribute
		/// GetException
		/// GetErrorMessage
		/// </see>
		public static Obj GetAttribute(Obj obj, string attribute) {
			unsafe {
				Obj r;
				fixed (byte* _attribute = attribute is null ? null : Encoding.ASCII.GetBytes(attribute + '\0')) {
					r = Wg_GetAttribute(obj, (IntPtr)_attribute);
				}
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_GetAttribute(Obj obj, IntPtr attribute);

		/// <summary>
		/// Get an attribute of an object.
		/// Unlike, GetAttribute(), this function does not raise exceptions.
		/// </summary>
		/// <param name="obj">
		/// The object to get the attribute from.
		/// </param>
		/// <param name="attribute">
		/// The attribute to get.
		/// </param>
		/// <returns>
		/// The attribute value, or null if the attribute does not exist.
		/// </returns>
		/// <see>
		/// HasAttribute
		/// GetAttribute
		/// GetAttributeFromBase
		/// SetAttribute
		/// </see>
		/// <remarks>
		/// This function will not bind unbound method objects to obj.
		/// </remarks>
		public static Obj GetAttributeNoExcept(Obj obj, string attribute) {
			unsafe {
				Obj r;
				fixed (byte* _attribute = attribute is null ? null : Encoding.ASCII.GetBytes(attribute + '\0')) {
					r = Wg_GetAttributeNoExcept(obj, (IntPtr)_attribute);
				}
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_GetAttributeNoExcept(Obj obj, IntPtr attribute);

		/// <summary>
		/// Set an attribute of an object.
		/// </summary>
		/// <param name="obj">
		/// The object to set the attribute for.
		/// </param>
		/// <param name="attribute">
		/// The attribute to set.
		/// </param>
		/// <param name="value">
		/// The attribute value.
		/// </param>
		/// <see>
		/// HasAttribute
		/// GetAttribute
		/// GetAttributeNoExcept
		/// GetAttributeFromBase
		/// </see>
		public static void SetAttribute(Obj obj, string attribute, Obj value) {
			unsafe {
				fixed (byte* _attribute = attribute is null ? null : Encoding.ASCII.GetBytes(attribute + '\0')) {
					Wg_SetAttribute(obj, (IntPtr)_attribute, value);
				}
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe void Wg_SetAttribute(Obj obj, IntPtr attribute, Obj value);

		/// <summary>
		/// Get an attribute of an object, skipping attributes that belong to the most derived layer.
		/// </summary>
		/// <param name="obj">
		/// The object to get the attribute from.
		/// </param>
		/// <param name="attribute">
		/// The attribute to get.
		/// </param>
		/// <param name="baseClass">
		/// The base class to search in, or null to search in all bases.
		/// </param>
		/// <returns>
		/// The attribute value, or null if the attribute does not exist.
		/// </returns>
		/// <see>
		/// HasAttribute
		/// GetAttribute
		/// GetAttributeNoExcept
		/// SetAttribute
		/// </see>
		public static Obj GetAttributeFromBase(Obj obj, string attribute, Obj baseClass = default) {
			unsafe {
				Obj r;
				fixed (byte* _attribute = attribute is null ? null : Encoding.ASCII.GetBytes(attribute + '\0')) {
					r = Wg_GetAttributeFromBase(obj, (IntPtr)_attribute, baseClass);
				}
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_GetAttributeFromBase(Obj obj, IntPtr attribute, Obj baseClass);

		/// <summary>
		/// Iterate over an iterable object.
		/// </summary>
		/// <param name="obj">
		/// The object to iterate over.
		/// </param>
		/// <param name="userdata">
		/// The userdata to be passed to the callback function.
		/// </param>
		/// <param name="callback">
		/// A function to be called for each value yielded by iteration.
		/// See IterationCallback for more details on this function.
		/// </param>
		/// <returns>
		/// True on success, or false on failure.
		/// </returns>
		/// <see>
		/// GetException
		/// GetErrorMessage
		/// </see>
		public static bool Iterate(Obj obj, IntPtr userdata, IterationCallback callback) {
			unsafe {
				bool r;
				r = Wg_Iterate(obj, userdata, callback) != 0;
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe byte Wg_Iterate(Obj obj, IntPtr userdata, IterationCallback callback);

		/// <summary>
		/// Helper function to unpack an iterable object into an array of objects.
		/// </summary>
		/// <param name="obj">
		/// The object to iterate over.
		/// </param>
		/// <param name="count">
		/// The expected number of values.
		/// </param>
		/// <param name="values">
		/// The unpacked objects.
		/// </param>
		/// <returns>
		/// A boolean indicating success.
		/// </returns>
		public static bool Unpack(Obj obj, int count, Obj[] values) {
			unsafe {
				bool r;
				r = Wg_Unpack(obj, count, values) != 0;
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe byte Wg_Unpack(Obj obj, int count, [In, Out] Obj[] values);

		/// <summary>
		/// Get the keyword arguments dictionary passed to the current function.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <returns>
		/// The keywords arguments dictionary.
		/// </returns>
		/// <see>
		/// Call
		/// CallMethod
		/// </see>
		/// <remarks>
		/// This function can return null to indicate an empty dictionary.
		/// </remarks>
		public static Obj GetKwargs(Context context) {
			unsafe {
				Obj r;
				r = Wg_GetKwargs(context);
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_GetKwargs(Context context);

		/// <summary>
		/// Get the userdata associated with the current function.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <returns>
		/// The userdata associated with the current function.
		/// </returns>
		/// <see>
		/// NewFunction
		/// BindMethod
		/// </see>
		/// <remarks>
		/// This function must be called inside a function bound with NewFunction() or BindMethod().
		/// </remarks>
		public static IntPtr GetFunctionUserdata(Context context) {
			unsafe {
				IntPtr r;
				r = Wg_GetFunctionUserdata(context);
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe IntPtr Wg_GetFunctionUserdata(Context context);

		/// <summary>
		/// Call a callable object.
		/// </summary>
		/// <param name="callable">
		/// The object to call.
		/// </param>
		/// <param name="argv">
		/// An array of arguments to pass to the callable object.
		/// If argc is 0 then this can be null.
		/// </param>
		/// <param name="argc">
		/// The length of the argv array.
		/// </param>
		/// <param name="kwargs">
		/// A dictionary object containing the keyword arguments or null if none.
		/// </param>
		/// <returns>
		/// The return value of the callable, or null on failure.
		/// </returns>
		/// <see>
		/// CallMethod
		/// GetException
		/// GetErrorMessage
		/// </see>
		public static Obj Call(Obj callable, Obj[] argv, int argc, Obj kwargs = default) {
			unsafe {
				Obj r;
				fixed (Obj* _argv = argv) {
					r = Wg_Call(callable, (IntPtr)_argv, argc, kwargs);
				}
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_Call(Obj callable, IntPtr argv, int argc, Obj kwargs);

		/// <summary>
		/// Call a method on a object.
		/// </summary>
		/// <param name="obj">
		/// The object to call the method on.
		/// </param>
		/// <param name="method">
		/// The method to call.
		/// </param>
		/// <param name="argv">
		/// An array of arguments to pass to the callable object.
		/// If argc is 0 then this can be null.
		/// </param>
		/// <param name="argc">
		/// The length of the argv array.
		/// </param>
		/// <param name="kwargs">
		/// A dictionary object containing the keyword arguments or null if none.
		/// </param>
		/// <returns>
		/// The return value of the callable, or null on failure.
		/// </returns>
		/// <see>
		/// Call
		/// GetException
		/// GetErrorMessage
		/// </see>
		public static Obj CallMethod(Obj obj, string method, Obj[] argv, int argc, Obj kwargs = default) {
			unsafe {
				Obj r;
				fixed (byte* _method = method is null ? null : Encoding.ASCII.GetBytes(method + '\0')) {
					fixed (Obj* _argv = argv) {
						r = Wg_CallMethod(obj, (IntPtr)_method, (IntPtr)_argv, argc, kwargs);
					}
				}
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_CallMethod(Obj obj, IntPtr method, IntPtr argv, int argc, Obj kwargs);

		/// <summary>
		/// Call a method on a object, skipping methods which belong to the most derived layer.
		/// </summary>
		/// <param name="obj">
		/// The object to call the method on.
		/// </param>
		/// <param name="method">
		/// The method to call.
		/// </param>
		/// <param name="argv">
		/// An array of arguments to pass to the callable object.
		/// If argc is 0 then this can be null.
		/// </param>
		/// <param name="argc">
		/// The length of the argv array.
		/// </param>
		/// <param name="kwargs">
		/// A dictionary object containing the keyword arguments or null if none.
		/// </param>
		/// <param name="baseClass">
		/// The base class to search in, or null to search in all bases.
		/// </param>
		/// <returns>
		/// The return value of the callable, or null on failure.
		/// </returns>
		/// <see>
		/// Call
		/// GetException
		/// GetErrorMessage
		/// </see>
		public static Obj CallMethodFromBase(Obj obj, string method, Obj[] argv, int argc, Obj kwargs = default, Obj baseClass = default) {
			unsafe {
				Obj r;
				fixed (byte* _method = method is null ? null : Encoding.ASCII.GetBytes(method + '\0')) {
					fixed (Obj* _argv = argv) {
						r = Wg_CallMethodFromBase(obj, (IntPtr)_method, (IntPtr)_argv, argc, kwargs, baseClass);
					}
				}
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_CallMethodFromBase(Obj obj, IntPtr method, IntPtr argv, int argc, Obj kwargs, Obj baseClass);

		/// <summary>
		/// Get the values from a kwargs parameter.
		/// </summary>
		/// <param name="dict">
		/// The dictionary to get the values from.
		/// If this is null, then an empty dictionary is assumed.
		/// </param>
		/// <param name="keys">
		/// The keys to look up.
		/// </param>
		/// <param name="keysLen">
		/// The length of the keys array.
		/// </param>
		/// <param name="values">
		/// The values in the dictionary corresponding to the keys.
		/// The values are given in the order that the keys are given
		/// and will be null for keys that were not found.
		/// </param>
		/// <returns>
		/// A boolean indicating whether the operation was successful.
		/// </returns>
		/// <see>
		/// GetException
		/// GetErrorMessage
		/// </see>
		/// <remarks>
		/// This function can be called with dictionaries
		/// other than the one returned by GetKwargs().
		/// </remarks>
		public static bool ParseKwargs(Obj dict, string[] keys, int keysLen, Obj[] values) {
			unsafe {
				bool r;
				var handles = keys.Select(x => GCHandle.Alloc(Encoding.ASCII.GetBytes(x + '\0'), GCHandleType.Pinned)).ToArray();
				fixed(void* _keys = handles.Select(x => x.AddrOfPinnedObject()).ToArray()) {
					r = Wg_ParseKwargs(dict, (IntPtr)_keys, keysLen, values) != 0;
					foreach (var handle in handles) handle.Free();
				}
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe byte Wg_ParseKwargs(Obj dict, IntPtr keys, int keysLen, [In, Out] Obj[] values);

		/// <summary>
		/// Index an object.
		/// </summary>
		/// <param name="obj">
		/// The object to index.
		/// </param>
		/// <param name="index">
		/// The index.
		/// </param>
		/// <returns>
		/// The object at the specified index, or null on failure.
		/// </returns>
		/// <see>
		/// UnaryOp
		/// BinaryOp
		/// SetIndex
		/// GetException
		/// GetErrorMessage
		/// </see>
		public static Obj GetIndex(Obj obj, Obj index) {
			unsafe {
				Obj r;
				r = Wg_GetIndex(obj, index);
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_GetIndex(Obj obj, Obj index);

		/// <summary>
		/// Set an index of an object.
		/// </summary>
		/// <param name="obj">
		/// The object to index.
		/// </param>
		/// <param name="index">
		/// The index.
		/// </param>
		/// <param name="value">
		/// The value to set.
		/// </param>
		/// <returns>
		/// The result of the operation (usually None), or null on failure.
		/// </returns>
		/// <see>
		/// UnaryOp
		/// BinaryOp
		/// GetIndex
		/// GetException
		/// GetErrorMessage
		/// </see>
		public static Obj SetIndex(Obj obj, Obj index, Obj value) {
			unsafe {
				Obj r;
				r = Wg_SetIndex(obj, index, value);
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_SetIndex(Obj obj, Obj index, Obj value);

		/// <summary>
		/// Perform a unary operation.
		/// </summary>
		/// <param name="op">
		/// The unary operation to perform.
		/// </param>
		/// <param name="arg">
		/// The operand.
		/// </param>
		/// <returns>
		/// The result of the operation, or null on failure.
		/// </returns>
		/// <see>
		/// GetException
		/// GetErrorMessage
		/// </see>
		public static Obj UnaryOp(UnOp op, Obj arg) {
			unsafe {
				Obj r;
				r = Wg_UnaryOp(op, arg);
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_UnaryOp(UnOp op, Obj arg);

		/// <summary>
		/// Perform a binary operation.
		/// </summary>
		/// <param name="op">
		/// The binary operation to perform.
		/// </param>
		/// <param name="lhs">
		/// The left hand side operand.
		/// </param>
		/// <param name="rhs">
		/// The right hand side operand.
		/// </param>
		/// <returns>
		/// The result of the operation, or null on failure.
		/// </returns>
		/// <see>
		/// GetException
		/// GetErrorMessage
		/// </see>
		/// <remarks>
		/// The logical and/or operators will short-circuit their truthy test.
		/// </remarks>
		public static Obj BinaryOp(BinOp op, Obj lhs, Obj rhs) {
			unsafe {
				Obj r;
				r = Wg_BinaryOp(op, lhs, rhs);
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_BinaryOp(BinOp op, Obj lhs, Obj rhs);

		/// <summary>
		/// Register a callback to be called when a module with the given name is imported.
		/// In the callback, new members can be added to the module.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <param name="name">
		/// The name of the module to register the callback for.
		/// </param>
		/// <param name="loader">
		/// The callback to register. The callback should return false on failure and otherwise true.
		/// </param>
		/// <see>
		/// ImportModule
		/// ImportFromModule
		/// ImportAllFromModule
		/// </see>
		public static void RegisterModule(Context context, string name, ModuleLoader loader) {
			unsafe {
				fixed (byte* _name = name is null ? null : Encoding.ASCII.GetBytes(name + '\0')) {
					Wg_RegisterModule(context, (IntPtr)_name, loader);
				}
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe void Wg_RegisterModule(Context context, IntPtr name, ModuleLoader loader);

		/// <summary>
		/// Import a module.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <param name="module">
		/// The name of the module to import.
		/// </param>
		/// <param name="alias">
		/// The alias to import the module under, or null to use the same name.
		/// </param>
		/// <returns>
		/// The imported module object, or null on failure.
		/// </returns>
		/// <see>
		/// RegisterModule
		/// ImportFromModule
		/// ImportAllFromModule
		/// GetException
		/// GetErrorMessage
		/// </see>
		/// <remarks>
		/// If the names in the module are rebound to different objects
		/// after importing, the changes are not visible to outside modules.
		/// </remarks>
		public static Obj ImportModule(Context context, string module, string? alias = default) {
			unsafe {
				Obj r;
				fixed (byte* _module = module is null ? null : Encoding.ASCII.GetBytes(module + '\0')) {
					fixed (byte* _alias = alias is null ? null : Encoding.ASCII.GetBytes(alias + '\0')) {
						r = Wg_ImportModule(context, (IntPtr)_module, (IntPtr)_alias);
					}
				}
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_ImportModule(Context context, IntPtr module, IntPtr alias);

		/// <summary>
		/// Import a specific name from a module.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <param name="module">
		/// The name of the module to import from.
		/// </param>
		/// <param name="name">
		/// The name to import.
		/// </param>
		/// <param name="alias">
		/// The alias to import the name under, or null to use the same name.
		/// </param>
		/// <returns>
		/// The imported object, or null on failure.
		/// </returns>
		/// <see>
		/// RegisterModule
		/// ImportModule
		/// ImportAllFromModule
		/// GetException
		/// GetErrorMessage
		/// </see>
		/// <remarks>
		/// If the names in the module are rebound to different objects
		/// after importing, the changes are not visible to outside modules.
		/// </remarks>
		public static Obj ImportFromModule(Context context, string module, string name, string? alias = default) {
			unsafe {
				Obj r;
				fixed (byte* _module = module is null ? null : Encoding.ASCII.GetBytes(module + '\0')) {
					fixed (byte* _name = name is null ? null : Encoding.ASCII.GetBytes(name + '\0')) {
						fixed (byte* _alias = alias is null ? null : Encoding.ASCII.GetBytes(alias + '\0')) {
							r = Wg_ImportFromModule(context, (IntPtr)_module, (IntPtr)_name, (IntPtr)_alias);
						}
					}
				}
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe Obj Wg_ImportFromModule(Context context, IntPtr module, IntPtr name, IntPtr alias);

		/// <summary>
		/// Import all names from a module.
		/// </summary>
		/// <param name="context">
		/// The associated context.
		/// </param>
		/// <param name="module">
		/// The name of the module to import.
		/// </param>
		/// <returns>
		/// A boolean indicating if the names were imported successfully.
		/// </returns>
		/// <see>
		/// RegisterModule
		/// ImportModule
		/// ImportFromModule
		/// GetException
		/// GetErrorMessage
		/// </see>
		/// <remarks>
		/// If the names in the module are rebound to different objects
		/// after importing, the changes are not visible to outside modules.
		/// </remarks>
		public static bool ImportAllFromModule(Context context, string module) {
			unsafe {
				bool r;
				fixed (byte* _module = module is null ? null : Encoding.ASCII.GetBytes(module + '\0')) {
					r = Wg_ImportAllFromModule(context, (IntPtr)_module) != 0;
				}
				return r;
			}
		}

		[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe byte Wg_ImportAllFromModule(Context context, IntPtr module);

	}
}
