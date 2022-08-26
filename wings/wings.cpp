#include "impl.h"
#include "gc.h"
#include "executor.h"
#include "compile.h"
#include <iostream>
#include <memory>
#include <string_view>
#include <atomic>
#include <mutex>

static WErrorCallback errorCallback;
static void* errorCallbackUserdata;
static std::mutex errorCallbackMutex;

extern "C" {
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
        wings::InitLibrary(context);
		
        // Apply possibly restrictive config now
        WSetConfig(context, config);
		
        return context;
    }

    void WDestroyContext(WContext* context) {
        WASSERT_VOID(context);
        wings::DestroyAllObjects(context);
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
            tag = wings::DEFAULT_TAG_NAME;

        auto lexResult = wings::Lex(code);
        auto originalSource = wings::MakeRcPtr<std::vector<std::string>>(lexResult.originalSource);

        auto raiseException = [&](const wings::CodeError& error) {
            context->currentTrace.push_back(wings::TraceFrame{
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
		
        auto* def = new wings::DefObject();
        def->context = context;
        def->tag = tag;
        def->prettyName = wings::DEFAULT_FUNC_NAME;
        def->originalSource = std::move(originalSource);
        auto instructions = Compile(parseResult.parseTree);
        def->instructions = MakeRcPtr<std::vector<wings::Instruction>>(std::move(instructions));

        WFuncDesc func{};
        func.fptr = &wings::DefObject::Run;
        func.userdata = def;
        func.prettyName = wings::DEFAULT_FUNC_NAME;
        WObj* obj = WCreateFunction(context, &func);
        if (obj == nullptr) {
            delete def;
            return nullptr;
        }

        WFinalizerDesc finalizer{};
        finalizer.fptr = [](WObj* obj, void* userdata) { delete (wings::DefObject*)userdata; };
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
            context->globals.insert({ std::string(name), wings::MakeRcPtr<WObj*>(value) });
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
