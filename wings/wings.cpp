#include "impl.h"
#include "gc.h"
#include "executor.h"
#include "compile.h"
#include <iostream>
#include <memory>
#include <string_view>
#include <atomic>
#include <mutex>

static Wg_ErrorCallback errorCallback;
static void* errorCallbackUserdata;
static std::mutex errorCallbackMutex;

extern "C" {
	void Wg_GetConfig(const Wg_Context* context, Wg_Config* out) {
		WASSERT_VOID(context && out);
		*out = context->config;
	}

	void Wg_SetConfig(Wg_Context* context, const Wg_Config* config) {
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

	Wg_Context* Wg_CreateContext(const Wg_Config* config) {
		Wg_Context* context = new Wg_Context();

		// Initialise the library without restriction
		Wg_SetConfig(context, nullptr);
		wings::InitLibrary(context);
		
		// Apply possibly restrictive config now
		Wg_SetConfig(context, config);
		
		return context;
	}

	void Wg_DestroyContext(Wg_Context* context) {
		WASSERT_VOID(context);
		wings::DestroyAllObjects(context);
		delete context;
	}

	void Wg_Print(const Wg_Context* context, const char* message, int len) {
		WASSERT_VOID(context && message);
		if (context->config.print) {
			context->config.print(len ? message : "", len, context->config.printUserdata);
		}
	}

	void Wg_PrintString(const Wg_Context* context, const char* message) {
		WASSERT_VOID(context && message);
		Wg_Print(context, message, (int)std::strlen(message));
	}

	void Wg_SetErrorCallback(Wg_ErrorCallback callback, void* userdata) {
		std::scoped_lock lock(errorCallbackMutex);
		errorCallback = callback;
		errorCallbackUserdata = userdata;
	}

	Wg_Obj* Wg_Compile(Wg_Context* context, const char* code, const char* tag) {
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

			Wg_RaiseException(
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

		Wg_FuncDesc func{};
		func.fptr = &wings::DefObject::Run;
		func.userdata = def;
		func.prettyName = wings::DEFAULT_FUNC_NAME;
		Wg_Obj* obj = Wg_CreateFunction(context, &func);
		if (obj == nullptr) {
			delete def;
			return nullptr;
		}

		Wg_FinalizerDesc finalizer{};
		finalizer.fptr = [](Wg_Obj* obj, void* userdata) { delete (wings::DefObject*)userdata; };
		finalizer.userdata = def;
		Wg_SetFinalizer(obj, &finalizer);

		return obj;
	}

	Wg_Obj* Wg_GetGlobal(Wg_Context* context, const char* name) {
		WASSERT(context && name);
		auto it = context->globals.find(std::string(name));
		if (it == context->globals.end()) {
			return nullptr;
		} else {
			return *it->second;
		}
	}

	void Wg_SetGlobal(Wg_Context* context, const char* name, Wg_Obj* value) {
		WASSERT_VOID(context && name && value);
		auto it = context->globals.find(std::string(name));
		if (it != context->globals.end()) {
			*it->second = value;
		} else {
			context->globals.insert({ std::string(name), wings::MakeRcPtr<Wg_Obj*>(value) });
		}
	}

	void Wg_DeleteGlobal(Wg_Context* context, const char* name) {
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
		return CodeError{ true };
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

} // namespace wings
