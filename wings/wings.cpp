#include "impl.h"
#include "gc.h"
#include "executor.h"
#include "compile.h"
#include <iostream>
#include <memory>
#include <sstream>
#include <string_view>
#include <algorithm>
#include <atomic>
#include <mutex>

using namespace wings;

static WErrorCallback errorCallback;
static void* errorCallbackUserdata;
static std::mutex errorCallbackMutex;

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

            if (frame.tag != DEFAULT_TAG_NAME) {
                ss << "Tag " << frame.tag;
                written = true;
            }

            if (frame.srcPos.line != (size_t)-1) {
                if (written) ss << ", ";
                ss << "Line " << (frame.srcPos.line + 1);
                written = true;
            }

            if (frame.func != DEFAULT_FUNC_NAME) {
                if (written) ss << ", ";
                ss << "Function " << frame.func << "()";
            }

            ss << "\n";

            if (!frame.lineText.empty()) {
                std::string lineText = frame.lineText;
                std::replace(lineText.begin(), lineText.end(), '\t', ' ');

                size_t skip = lineText.find_first_not_of(' ');
                ss << "    " << (lineText.c_str() + skip) << "\n";
                ss << std::string(frame.srcPos.column + 4 - skip, ' ') << "^\n";
            }
        }

        ss << context->currentException->type;
        if (WObj* msg = WGetAttribute(context->currentException, "message"))
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
            WRaiseException(context, "exceptions must derive from BaseException", context->builtins.typeError);
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
        WRaiseException(context, msg.c_str(), context->builtins.typeError);
    }

    void WRaiseArgumentTypeError(WContext* context, int argIndex, const char* expected) {
		WASSERT_VOID(context && argIndex >= 0 && expected);
        std::string msg = "Argument " + std::to_string(argIndex + 1)
            + " Expected type " + expected;
        WRaiseException(context, msg.c_str(), context->builtins.typeError);
    }

    void WRaiseAttributeError(const WObj* obj, const char* attribute) {
        WASSERT_VOID(obj && attribute);
        std::string msg = "'" + WObjTypeToString(obj) +
            "' object has no attribute '" + attribute + "'";
        WRaiseException(obj->context, msg.c_str(), obj->context->builtins.attributeError);
    }

    void WGetConfig(const WContext* context, WConfig* out) {
        WASSERT_VOID(context && out);
        *out = context->config;
    }

    void WSetConfig(WContext* context, const WConfig* config) {
        WASSERT_VOID(context);
        if (config) {
            WASSERT_VOID(config->maxAlloc >= 0);
            WASSERT_VOID(config->maxRecursion >= 0);
            WASSERT_VOID(config->maxCollectionSize >= 0);
            WASSERT_VOID(config->gcRunFactor >= 1.0f);
            context->config = *config;
        } else {
            context->config.maxAlloc = 100'000;
            context->config.maxRecursion = 100;
            context->config.maxCollectionSize = 1'000'000'000;
            context->config.gcRunFactor = 2.0f;
            context->config.print = [](const char* message, int len, void*) {
                std::cout << std::string_view(message, (size_t)len);
            };
        }
    }

    WContext* WCreateContext(const WConfig* config) {
        WContext* context = new WContext();

        // Initialise the library without restriction
        WSetConfig(context, nullptr);
        InitLibrary(context);
		
        // Apply possibly restrictive config now
        WSetConfig(context, config);
		
        return context;
    }

    void WDestroyContext(WContext* context) {
        WASSERT_VOID(context);
        DestroyAllObjects(context);
        delete context;
    }

    void WPrint(const WContext* context, const char* message, int len) {
        WASSERT_VOID(context && message);
        if (context->config.print) {
            context->config.print(len ? message : "", len, context->config.printUserdata);
        }
    }

    void WPrintString(const WContext* context, const char* message) {
        WASSERT_VOID(context && message);
        WPrint(context, message, (int)std::strlen(message));
    }

    void WSetErrorCallback(WErrorCallback callback, void* userdata) {
        std::scoped_lock lock(errorCallbackMutex);
        errorCallback = callback;
        errorCallbackUserdata = userdata;
    }

    WObj* WCompile(WContext* context, const char* code, const char* tag) {
        WASSERT(context && code);

        if (tag == nullptr)
            tag = DEFAULT_TAG_NAME;

        auto lexResult = Lex(code);
        auto originalSource = MakeRcPtr<std::vector<std::string>>(lexResult.originalSource);

        auto raiseException = [&](const CodeError& error) {
            context->currentTrace.push_back(TraceFrame{
                error.srcPos,
                (*originalSource)[error.srcPos.line],
                tag,
                tag
                });

            WRaiseException(
                context,
                error.message.c_str(),
                context->builtins.syntaxError
            );
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
		
        DefObject* def = new DefObject();
        def->context = context;
        def->tag = tag;
        def->prettyName = DEFAULT_FUNC_NAME;
        def->originalSource = std::move(originalSource);
        auto instructions = Compile(parseResult.parseTree);
        def->instructions = MakeRcPtr<std::vector<Instruction>>(std::move(instructions));

        WFuncDesc func{};
        func.fptr = &DefObject::Run;
        func.userdata = def;
        func.prettyName = DEFAULT_FUNC_NAME;
        WObj* obj = WCreateFunction(context, &func);
        if (obj == nullptr) {
            delete def;
            return nullptr;
        }

        WFinalizerDesc finalizer{};
        finalizer.fptr = [](WObj* obj, void* userdata) { delete (DefObject*)userdata; };
        finalizer.userdata = def;
        WSetFinalizer(obj, &finalizer);

        return obj;
    }

    WObj* WGetGlobal(WContext* context, const char* name) {
        WASSERT(context && name);
        auto it = context->globals.find(std::string(name));
        if (it == context->globals.end()) {
            return nullptr;
        } else {
            return *it->second;
        }
    }

    void WSetGlobal(WContext* context, const char* name, WObj* value) {
        WASSERT_VOID(context && name && value);
        auto it = context->globals.find(std::string(name));
        if (it != context->globals.end()) {
            *it->second = value;
        } else {
            context->globals.insert({ std::string(name), MakeRcPtr<WObj*>(value) });
        }
    }

    void WDeleteGlobal(WContext* context, const char* name) {
        WASSERT_VOID(context && name);
        auto it = context->globals.find(std::string(name));
        if (it != context->globals.end()) {
            context->globals.erase(it);
        }
    }

} // extern "C"

namespace wings {

    void CallErrorCallback(const char* message) {
        errorCallbackMutex.lock();
        auto cb = errorCallback;
        auto ud = errorCallbackUserdata;
        errorCallbackMutex.unlock();

        if (cb) {
            cb(message, ud);
        } else {
            std::abort();
        }
    }

    size_t Guid() {
        static std::atomic_size_t i = 0;
        return ++i;
    }

    std::string WObjTypeToString(const WObj* obj) {
        if (WIsNone(obj)) {
            return "NoneType";
        } else if (WIsBool(obj)) {
            return "bool";
        } else if (WIsInt(obj)) {
            return "int";
        } else if (WIsIntOrFloat(obj)) {
            return "float";
        } else if (WIsString(obj)) {
            return "str";
        } else if (WIsTuple(obj)) {
            return "tuple";
        } else if (WIsList(obj)) {
            return "list";
        } else if (WIsDictionary(obj)) {
            return "dict";
        } else if (WIsFunction(obj)) {
            return "function";
        } else if (WIsClass(obj)) {
            return "class";
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
        return CodeError{ true };
    }

    CodeError CodeError::Bad(std::string message, SourcePosition srcPos) {
        return CodeError{
            .good = false,
            .srcPos = srcPos,
            .message = message
        };
    }

} // namespace wings
