#include "impl.h"
#include "executor.h"
#include "compile.h"
#include <iostream>
#include <memory>
#include <sstream>

using namespace wings;

extern "C" {

    WError WErrorGet() {
        return werror;
    }

    const char* WErrorMessageGet() {
        switch (werror) {
        case WError::WERROR_OK: return "Ok";
        case WError::WERROR_MAX_ALLOC: return "Exceeded maximum number of objects allocated at a time";
        case WError::WERROR_MAX_RECURSION: return "Exceeded maximum recursion limit";
        default: return werrorMessage.c_str();
        }
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

    WObj* WContextCompile(WContext* context, const char* code) {

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
            werror = WError::WERROR_COMPILE_FAILED;
            werrorMessage = formatError(lexResult.error, lexResult.rawCode);
            return nullptr;
        }

        auto parseResult = Parse(lexResult.lexTree);
        if (parseResult.error) {
            werror = WError::WERROR_COMPILE_FAILED;
            werrorMessage = formatError(parseResult.error, lexResult.rawCode);
            return nullptr;
        }

        Executor* executor = new Executor();
        executor->instructions = Compile(parseResult.parseTree);

        WFunc func{};
        func.fptr = &Executor::Run;
        func.userdata = executor;
        WObj* obj = WObjCreateFunc(context, &func);
        if (obj == nullptr) {
            delete (Executor*)executor;
            return nullptr;
        }

        WFinalizer finalizer{};
        finalizer.fptr = [](WObj* obj, void* userdata) { delete (Executor*)userdata; };
        finalizer.userdata = executor;
        WObjSetFinalizer(obj, &finalizer);

        return obj;
    }

} // extern "C"

namespace wings {
    size_t Guid() {
        static size_t i = 0;
        return ++i;
    }
}
