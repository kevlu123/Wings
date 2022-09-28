#include "wings.h"
#include "impl.h"
#include <sstream>
#include <algorithm>
#include <ranges>

extern "C" {
    const char* Wg_GetErrorMessage(Wg_Context* context) {
        WASSERT(context);

        if (context->currentException == nullptr) {
            return (context->traceMessage = "Ok").c_str();
        }

        std::stringstream ss;
        ss << "Traceback (most recent call last):\n";

        for (const auto& frame : context->exceptionTrace) {
            if (frame.tag == "__builtins__")
                continue;

            ss << "  ";
            bool written = false;

            if (frame.tag != wings::DEFAULT_TAG_NAME) {
                ss << "Tag " << frame.tag;
                written = true;
            }

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
                //if (skip <= frame.srcPos.column)
                //    ss << std::string(frame.srcPos.column + 4 - skip, ' ') << "^\n";
            }
        }

        ss << context->currentException->type;
        if (Wg_Obj* msg = Wg_HasAttribute(context->currentException, "_message"))
            if (Wg_IsString(msg))
                ss << ": " << Wg_GetString(msg);
        ss << "\n";

        context->traceMessage = ss.str();
        return context->traceMessage.c_str();
    }

    Wg_Obj* Wg_GetCurrentException(Wg_Context* context) {
        WASSERT(context);
        return context->currentException;
    }

    void Wg_ClearCurrentException(Wg_Context* context) {
        WASSERT_VOID(context);
        context->currentException = nullptr;
        context->exceptionTrace.clear();
        context->traceMessage.clear();
    }

    void Wg_RaiseException(Wg_Context* context, Wg_Exc type, const char* message) {
        WASSERT_VOID(context);
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
            return Wg_RaiseExceptionClass(context->builtins.memoryError, message);
        case WG_EXC_NAMEERROR:
            return Wg_RaiseExceptionClass(context->builtins.nameError, message);
        case WG_EXC_RUNTIMEERROR:
            return Wg_RaiseExceptionClass(context->builtins.runtimeError, message);
        case WG_EXC_NOTIMPLEMENTEDERROR:
            return Wg_RaiseExceptionClass(context->builtins.notImplementedError, message);
        case WG_EXC_RECURSIONERROR:
            return Wg_RaiseExceptionClass(context->builtins.recursionError, message);
        case WG_EXC_SYNTAXERROR:
            return Wg_RaiseExceptionClass(context->builtins.syntaxError, message);
        case WG_EXC_TYPEERROR:
            return Wg_RaiseExceptionClass(context->builtins.typeError, message);
        case WG_EXC_VALUEERROR:
            return Wg_RaiseExceptionClass(context->builtins.valueError, message);
        default:
            WASSERT_VOID(false);
        }
    }

    void Wg_RaiseExceptionClass(Wg_Obj* type, const char* message) {
        WASSERT_VOID(type);
        wings::WObjRef ref(type);

        Wg_Obj* msg = Wg_CreateString(type->context, message);
        if (msg == nullptr) {
            return;
        }

        // If exception creation was successful then set the exception.
        // Otherwise the exception will already be set by some other code.
        if (Wg_Obj* exceptionObject = Wg_Call(type, &msg, msg ? 1 : 0)) {
            Wg_RaiseExceptionObject(exceptionObject);
        }
    }

    void Wg_RaiseExceptionObject(Wg_Obj* exception) {
        WASSERT_VOID(exception);
        Wg_Context* context = exception->context;
        if (Wg_IsInstance(exception, &context->builtins.baseException, 1)) {
            context->currentException = exception;
            context->exceptionTrace.clear();
            for (const auto& frame : context->currentTrace)
                context->exceptionTrace.push_back(frame.ToOwned());
        } else {
            Wg_RaiseException(context, WG_EXC_TYPEERROR, "exceptions must derive from BaseException");
        }
    }

    void Wg_RaiseArgumentCountError(Wg_Context* context, int given, int expected) {
        WASSERT_VOID(context && given >= 0 && expected >= -1);
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

    void Wg_RaiseArgumentTypeError(Wg_Context* context, int argIndex, const char* expected) {
        WASSERT_VOID(context && argIndex >= 0 && expected);
        std::string msg = "Argument " + std::to_string(argIndex + 1)
            + " Expected type " + expected;
        Wg_RaiseException(context, WG_EXC_TYPEERROR, msg.c_str());
    }

    void Wg_RaiseAttributeError(const Wg_Obj* obj, const char* attribute) {
        WASSERT_VOID(obj && attribute);
        std::string msg = "'" + wings::WObjTypeToString(obj) +
            "' object has no attribute '" + attribute + "'";
        Wg_RaiseException(obj->context, WG_EXC_ATTRIBUTEERROR, msg.c_str());
    }

    void Wg_RaiseZeroDivisionError(Wg_Context* context) {
        WASSERT_VOID(context);
        Wg_RaiseException(context, WG_EXC_ZERODIVISIONERROR, "division by zero");
    }

    void Wg_RaiseIndexError(Wg_Context* context) {
        WASSERT_VOID(context);
        Wg_RaiseException(context, WG_EXC_INDEXERROR, "index out of range");
    }

    void Wg_RaiseKeyError(Wg_Context* context, Wg_Obj* key) {
        WASSERT_VOID(context);

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
        WASSERT_VOID(context && name);
        std::string msg = "The name '";
        msg += name;
        msg += "' is not defined";
        Wg_RaiseException(context, WG_EXC_NAMEERROR, msg.c_str());
    }
}
