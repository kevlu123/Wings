 #include "impl.h"
#include "executor.h"
#include "compile.h"
#include <iostream>
#include <memory>
#include <sstream>

using namespace wings;

static std::string CreateTracebackMessage(WContext* context) {
    std::stringstream ss;
    ss << "Traceback (most recent call last):\n";

    for (const auto& frame : context->err.trace) {
        ss << "  ";
        bool written = false;

        if (!frame.module.empty()) {
            ss << "Module " << frame.module;
            written = true;
        }

        if (frame.line != 0) {
            if (written) ss << ", ";
            ss << "Line " << frame.line;
            written = true;
        }

        if (!frame.func.empty()) {
            if (written) ss << ", ";
            ss << "Function " << frame.func << "()";
        }

        ss << "\n";
    }

    ss << context->err.message << "\n";
    return ss.str();

    //std::string s = "Traceback (most recent call last):\n";
    //for (const auto& frame : context->err.trace) {
    //    s += "  Module \"";
    //    s += frame.module;
    //    s += "\", line " + std::to_string(frame.line);
    //    if (!frame.func.empty()) {
    //        s += ", in ";
    //        s += frame.func;
    //    } else {
    //        s += ", in <no name>";
    //    }
    //    s += "\n";
    //}
    //s += context->err.message + "\n";
    //return s;
}

static void SetCompileError(WContext* context, const std::string& message) {
    context->err.code = WError::WERROR_COMPILE_FAILED;
    context->err.message = message;
}

extern "C" {

    WError WErrorGet(WContext* context) {
        WASSERT(context);
        return context->err.code;
    }

    const char* WErrorMessageGet(WContext* context) {
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

    void WErrorSetRuntimeError(WContext* context, const char* message) {
        WASSERT(context && message);
        context->err.code = WError::WERROR_RUNTIME_ERROR;
        context->err.message = message;
    }

    void WContextGetConfig(const WContext* context, WConfig* config) {
        WASSERT(context && config);
        *config = context->config;
    }

    void WContextSetConfig(WContext* context, const WConfig* config) {
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
            context->config.log = [](const char* message) { std::cout << message << '\n'; };
        }
    }

    WContext* WContextCreate(const WConfig* config) {
        WContext* context = new WContext();
        WContextSetConfig(context, config);
        if (!InitLibrary(context)) {
            WContextDestroy(context);
            return nullptr;
        }
        return context;
    }

    void WContextDestroy(WContext* context) {
        if (context) {
            delete context;
        }
    }

    void WContextLog(const WContext* context, const char* message) {
        WASSERT(context && message);
        if (context->config.log)
            context->config.log(message);
    }

    WObj* WContextCompile(WContext* context, const char* code, const char* moduleName) {
        WASSERT(context && code && moduleName);

        auto formatError = [](const auto& err, const auto& rawCode) {
            std::stringstream ss;
            ss << "Error on line " << (err.srcPos.line + 1) << '\n';
            ss << rawCode[err.srcPos.line] << '\n';
            ss << std::string(err.srcPos.column, ' ') << "^\n";
            ss << err.message;
            return ss.str();
        };

        auto lexResult = Lex(code);
        if (lexResult.error) {
            SetCompileError(context, formatError(lexResult.error, lexResult.rawCode));
            return nullptr;
        }

        auto parseResult = Parse(lexResult.lexTree);
        if (parseResult.error) {
            SetCompileError(context, formatError(parseResult.error, lexResult.rawCode));
            return nullptr;
        }

        DefObject* def = new DefObject();
        def->context = context;
        def->module = moduleName;
        def->prettyName = "";
        auto instructions = Compile(parseResult.parseTree);
        def->instructions = MakeRcPtr<std::vector<Instruction>>(std::move(instructions));

        WFunc func{};
        func.fptr = &DefObject::Run;
        func.userdata = def;
        func.prettyName = "";
        WObj* obj = WObjCreateFunc(context, &func);
        if (obj == nullptr) {
            delete def;
            return nullptr;
        }

        WFinalizer finalizer{};
        finalizer.fptr = [](WObj* obj, void* userdata) { delete (DefObject*)userdata; };
        finalizer.userdata = def;
        WObjSetFinalizer(obj, &finalizer);

        return obj;
    }

    WObj* WContextGetGlobal(WContext* context, const char* name) {
        auto it = context->globals.find(std::string(name));
        if (it == context->globals.end()) {
            return nullptr;
        } else {
            return *it->second;
        }
    }

    void WContextSetGlobal(WContext* context, const char* name, WObj* value) {
        if (context->globals.contains(std::string(name))) {
            *context->globals.at(std::string(name)) = value;
        } else {
            context->globals.insert({ std::string(name), MakeRcPtr<WObj*>(value) });
        }
    }

} // extern "C"

namespace wings {

    size_t Guid() {
        static size_t i = 0;
        return ++i;
    }

    std::string WObjTypeToString(WObj::Type t) {
        switch (t) {
        case WObj::Type::Null: return "NoneType";
        case WObj::Type::Bool: return "bool";
        case WObj::Type::Int: return "int";
        case WObj::Type::Float: return "float";
        case WObj::Type::String: return "str";
        case WObj::Type::List: return "list";
        case WObj::Type::Map: return "map";
        case WObj::Type::Object: return "object";
        case WObj::Type::Func: return "function";
        case WObj::Type::Userdata: return "userdata";
        default: WUNREACHABLE();
        }
    }

} // namespace wings
