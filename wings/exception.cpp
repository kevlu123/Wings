#include "wings.h"
#include "impl.h"
#include <sstream>
#include <algorithm>

extern "C" {
    const char* WGetErrorMessage(WContext* context) {
        WASSERT(context);

        if (context->currentException == nullptr) {
            return (context->traceMessage = "Ok").c_str();
        }

        std::stringstream ss;
        ss << "Traceback (most recent call first):\n";

        for (const auto& frame : context->exceptionTrace) {
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
                if (skip <= frame.srcPos.column)
                    ss << std::string(frame.srcPos.column + 4 - skip, ' ') << "^\n";
            }
        }

        ss << context->currentException->type;
        if (WObj* msg = WHasAttribute(context->currentException, "_message"))
            if (WIsString(msg))
                ss << ": " << WGetString(msg);
        ss << "\n";

        context->traceMessage = ss.str();
        return context->traceMessage.c_str();
    }

    WObj* WGetCurrentException(WContext* context) {
        WASSERT(context);
        return context->currentException;
    }

    void WClearCurrentException(WContext* context) {
        WASSERT_VOID(context);
        context->currentException = nullptr;
        context->exceptionTrace.clear();
        context->traceMessage.clear();
    }

    void WRaiseException(WContext* context, const char* message, WObj* type) {
        WASSERT_VOID(context);
        type = type ? type : context->builtins.exception;
        wings::WObjRef ref(type);

        WObj* msg = WCreateString(context, message);
        if (msg == nullptr) {
            return;
        }

        // If exception creation was successful then set the exception.
        // Otherwise the exception will already be set by some other code.
        if (WObj* exceptionObject = WCall(type, &msg, 1)) {
            WRaiseExceptionObject(context, exceptionObject);
        }
    }

    void WRaiseExceptionObject(WContext* context, WObj* exception) {
        WASSERT_VOID(context && exception);
        if (WIsInstance(exception, &context->builtins.baseException, 1)) {
            context->currentException = exception;
            context->exceptionTrace.clear();
            for (const auto& frame : context->currentTrace)
                context->exceptionTrace.push_back(frame.ToOwned());
        } else {
            WRaiseTypeError(context, "exceptions must derive from BaseException");
        }
    }

    void WRaiseArgumentCountError(WContext* context, int given, int expected) {
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
        WRaiseTypeError(context, msg.c_str());
    }

    void WRaiseArgumentTypeError(WContext* context, int argIndex, const char* expected) {
        WASSERT_VOID(context && argIndex >= 0 && expected);
        std::string msg = "Argument " + std::to_string(argIndex + 1)
            + " Expected type " + expected;
        WRaiseTypeError(context, msg.c_str());
    }

    void WRaiseAttributeError(const WObj* obj, const char* attribute) {
        WASSERT_VOID(obj && attribute);
        std::string msg = "'" + wings::WObjTypeToString(obj) +
            "' object has no attribute '" + attribute + "'";
        WRaiseException(obj->context, msg.c_str(), obj->context->builtins.attributeError);
    }

    void WRaiseZeroDivisionError(WContext* context) {
        WASSERT_VOID(context);
        WRaiseException(context, "division by zero", context->builtins.zeroDivisionError);
    }

    void WRaiseIndexError(WContext* context) {
        WASSERT_VOID(context);
        WRaiseException(context, "index out of range", context->builtins.indexError);
    }

    void WRaiseKeyError(WContext* context, WObj* key) {
        WASSERT_VOID(context && key);

        std::string s = "<exception str() failed>";
        if (WObj* repr = WRepr(key))
            s = WGetString(repr);

        WRaiseException(context, s.c_str(), context->builtins.keyError);
    }

    void WRaiseOverflowError(WContext* context) {
        WASSERT_VOID(context);
        WRaiseException(context, "index out of range", context->builtins.overflowError);
    }

    void WRaiseStopIteration(WContext* context) {
        WASSERT_VOID(context);
        WRaiseException(context, nullptr, context->builtins.stopIteration);
    }

    void WRaiseTypeError(WContext* context, const char* message) {
        WASSERT_VOID(context);
        WRaiseException(context, message, context->builtins.typeError);
    }

    void WRaiseValueError(WContext* context, const char* message) {
        WASSERT_VOID(context);
        WRaiseException(context, message, context->builtins.valueError);
    }

    void WRaiseNameError(WContext* context, const char* name) {
        WASSERT_VOID(context && name);
        std::string msg = "The name '";
        msg += name;
        msg += "' is not defined";
        WRaiseException(context, msg.c_str(), context->builtins.nameError);
    }
}
