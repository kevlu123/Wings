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

    void Wg_RaiseException(Wg_Context* context, const char* message, Wg_Obj* type) {
        WASSERT_VOID(context);
        type = type ? type : context->builtins.exception;
        wings::WObjRef ref(type);

        Wg_Obj* msg = Wg_CreateString(context, message);
        if (msg == nullptr) {
            return;
        }

        // If exception creation was successful then set the exception.
        // Otherwise the exception will already be set by some other code.
        if (Wg_Obj* exceptionObject = Wg_Call(type, &msg, 1)) {
            Wg_RaiseExceptionObject(context, exceptionObject);
        }
    }

    void Wg_RaiseExceptionObject(Wg_Context* context, Wg_Obj* exception) {
        WASSERT_VOID(context && exception);
        if (Wg_IsInstance(exception, &context->builtins.baseException, 1)) {
            context->currentException = exception;
            context->exceptionTrace.clear();
            for (const auto& frame : context->currentTrace)
                context->exceptionTrace.push_back(frame.ToOwned());
        } else {
            Wg_RaiseTypeError(context, "exceptions must derive from BaseException");
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
        Wg_RaiseTypeError(context, msg.c_str());
    }

    void Wg_RaiseArgumentTypeError(Wg_Context* context, int argIndex, const char* expected) {
        WASSERT_VOID(context && argIndex >= 0 && expected);
        std::string msg = "Argument " + std::to_string(argIndex + 1)
            + " Expected type " + expected;
        Wg_RaiseTypeError(context, msg.c_str());
    }

    void Wg_RaiseAttributeError(const Wg_Obj* obj, const char* attribute) {
        WASSERT_VOID(obj && attribute);
        std::string msg = "'" + wings::WObjTypeToString(obj) +
            "' object has no attribute '" + attribute + "'";
        Wg_RaiseException(obj->context, msg.c_str(), obj->context->builtins.attributeError);
    }

    void Wg_RaiseZeroDivisionError(Wg_Context* context) {
        WASSERT_VOID(context);
        Wg_RaiseException(context, "division by zero", context->builtins.zeroDivisionError);
    }

    void Wg_RaiseIndexError(Wg_Context* context) {
        WASSERT_VOID(context);
        Wg_RaiseException(context, "index out of range", context->builtins.indexError);
    }

    void Wg_RaiseKeyError(Wg_Context* context, Wg_Obj* key) {
        WASSERT_VOID(context);

        if (key == nullptr) {
            Wg_RaiseException(context, nullptr, context->builtins.keyError);
        } else {
            std::string s = "<exception str() failed>";
            if (Wg_Obj* repr = Wg_UnaryOp(WG_UOP_REPR, key))
                s = Wg_GetString(repr);
            Wg_RaiseException(context, s.c_str(), context->builtins.keyError);
        }
    }

    void WRaiseOverflowError(Wg_Context* context) {
        WASSERT_VOID(context);
        Wg_RaiseException(context, "index out of range", context->builtins.overflowError);
    }

    void Wg_RaiseStopIteration(Wg_Context* context) {
        WASSERT_VOID(context);
        Wg_RaiseException(context, nullptr, context->builtins.stopIteration);
    }

    void Wg_RaiseTypeError(Wg_Context* context, const char* message) {
        WASSERT_VOID(context);
        Wg_RaiseException(context, message, context->builtins.typeError);
    }

    void Wg_RaiseValueError(Wg_Context* context, const char* message) {
        WASSERT_VOID(context);
        Wg_RaiseException(context, message, context->builtins.valueError);
    }

    void Wg_RaiseNameError(Wg_Context* context, const char* name) {
        WASSERT_VOID(context && name);
        std::string msg = "The name '";
        msg += name;
        msg += "' is not defined";
        Wg_RaiseException(context, msg.c_str(), context->builtins.nameError);
    }

    void Wg_RaiseSystemExit(Wg_Context* context) {
        WASSERT_VOID(context);
        Wg_RaiseException(context, nullptr, context->builtins.systemExit);
    }
}
