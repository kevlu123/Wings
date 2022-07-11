#include "impl.h"
#include "gc.h"
#include "executor.h"
#include "compile.h"
#include <iostream>
#include <memory>
#include <sstream>
#include <algorithm>
#include <atomic>

using namespace wings;

static std::string CreateTracebackMessage(WContext* context) {
    std::stringstream ss;
    ss << "Traceback (most recent call first):\n";

    for (const auto& frame : context->err.trace) {
        ss << "  ";
        bool written = false;

        if (!frame.module.empty()) {
            ss << "Module " << frame.module;
            written = true;
        }

        if (frame.srcPos.line != (size_t)-1) {
            if (written) ss << ", ";
            ss << "Line " << (frame.srcPos.line + 1); 
            written = true;
        }

        if (!frame.func.empty()) {
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

    ss << context->err.message << "\n";
    return ss.str();
}

static void SetCompileError(WContext* context, const std::string& message) {
    context->err.code = WError::WERROR_COMPILE_FAILED;
    context->err.message = message;
}

extern "C" {

    WError WGetErrorCode(WContext* context) {
        WASSERT(context);
        return context->err.code;
    }

    const char* WGetErrorMessage(WContext* context) {
        WASSERT(context);
        switch (context->err.code) {
        case WError::WERROR_OK: return "Ok";
        case WError::WERROR_MAX_ALLOC: return "Exceeded maximum number of objects allocated at a time";
        case WError::WERROR_MAX_RECURSION: return "Exceeded maximum recursion limit";
        case WError::WERROR_COMPILE_FAILED: return context->err.message.c_str();
        case WError::WERROR_RUNTIME_ERROR: return (context->err.traceMessage = CreateTracebackMessage(context)).c_str();
        default: WUNREACHABLE();
        }
    }

    void WRaiseError(WContext* context, const char* message) {
        WASSERT(context && message);
        context->err.code = WError::WERROR_RUNTIME_ERROR;
        context->err.message = message;
    }

    void WClearError(WContext* context) {
        context->err.code = WError::WERROR_OK;
        context->err.message.clear();
        context->err.trace.clear();
        context->err.traceMessage.clear();
    }

    WObj* WGetCurrentException(WContext* context) {
        return context->currentException;
    }

    void WClearCurrentException(WContext* context) {
        context->currentException = nullptr;
    }

    void WGetConfig(const WContext* context, WConfig* config) {
        WASSERT(context && config);
        *config = context->config;
    }

    void WSetConfig(WContext* context, const WConfig* config) {
        WASSERT(context);
        if (config) {
            WASSERT(config->maxAlloc >= 0);
            WASSERT(config->maxRecursion >= 0);
            WASSERT(config->maxCollectionSize >= 0);
            WASSERT(config->gcRunFactor >= 1.0f);
            context->config = *config;
        } else {
            context->config.maxAlloc = 100'000;
            context->config.maxRecursion = 100;
            context->config.maxCollectionSize = 1'000'000'000;
            context->config.gcRunFactor = 2.0f;
            context->config.print = [](const char* message, int len) { std::cout << std::string(message, (size_t)len); };
        }
    }

    bool WCreateContext(WContext** context, const WConfig* config) {
        *context = new WContext();
        WSetConfig(*context, config);
        return InitLibrary(*context);
    }

    void WDestroyContext(WContext* context) {
        if (context) {
            DestroyAllObjects(context);
            delete context;
        }
    }

    void WPrint(const WContext* context, const char* message, int len) {
        WASSERT(context && message);
        if (context->config.print) {
            context->config.print(message, len);
        }
    }

    void WPrintString(const WContext* context, const char* message) {
        WPrint(context, message, (int)std::strlen(message));
    }

    WObj* WCompile(WContext* context, const char* code, const char* moduleName) {
        WASSERT(context && code);

        auto formatError = [](const auto& err, const auto& rawCode) {
            std::stringstream ss;
            ss << "Error on line " << (err.srcPos.line + 1) << '\n';
            ss << rawCode[err.srcPos.line] << '\n';
            ss << std::string(err.srcPos.column, ' ') << "^\n";
            ss << err.message;
            return ss.str();
        };

        auto lexResult = Lex(code);
        auto originalSource = MakeRcPtr<std::vector<std::string>>(lexResult.originalSource);
        if (lexResult.error) {
            SetCompileError(context, formatError(lexResult.error, *originalSource));
            return nullptr;
        }

        auto parseResult = Parse(lexResult.lexTree);
        if (parseResult.error) {
            SetCompileError(context, formatError(parseResult.error, *originalSource));
            return nullptr;
        }

        DefObject* def = new DefObject();
        def->context = context;
        def->module = moduleName ? moduleName : "<unnamed>";
        def->prettyName = "";
        def->originalSource = std::move(originalSource);
        auto instructions = Compile(parseResult.parseTree);
        def->instructions = MakeRcPtr<std::vector<Instruction>>(std::move(instructions));

        WFunc func{};
        func.fptr = &DefObject::Run;
        func.userdata = def;
        func.prettyName = "";
        WObj* obj = WCreateFunction(context, &func);
        if (obj == nullptr) {
            delete def;
            return nullptr;
        }

        WFinalizer finalizer{};
        finalizer.fptr = [](WObj* obj, void* userdata) { delete (DefObject*)userdata; };
        finalizer.userdata = def;
        WSetFinalizer(obj, &finalizer);

        return obj;
    }

    WObj* WGetGlobal(WContext* context, const char* name) {
        auto it = context->globals.find(std::string(name));
        if (it == context->globals.end()) {
            return nullptr;
        } else {
            return *it->second;
        }
    }

    void WSetGlobal(WContext* context, const char* name, WObj* value) {
        if (context->globals.contains(std::string(name))) {
            *context->globals.at(std::string(name)) = value;
        } else {
            context->globals.insert({ std::string(name), MakeRcPtr<WObj*>(value) });
        }
    }

} // extern "C"

namespace wings {

    size_t Guid() {
        static std::atomic_size_t i = 0;
        return ++i;
    }

    std::string WObjTypeToString(WObj::Type t) {
        switch (t) {
        case WObj::Type::Null: return "NoneType";
        case WObj::Type::Bool: return "bool";
        case WObj::Type::Int: return "int";
        case WObj::Type::Float: return "float";
        case WObj::Type::String: return "str";
        case WObj::Type::Tuple: return "tuple";
        case WObj::Type::List: return "list";
        case WObj::Type::Map: return "dict";
        case WObj::Type::Object: return "object";
        case WObj::Type::Func: return "function";
        case WObj::Type::Userdata: return "userdata";
        case WObj::Type::Class: return "class";
        default: WUNREACHABLE();
        }
    }

} // namespace wings
