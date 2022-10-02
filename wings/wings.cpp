#include "impl.h"
#include "gc.h"
#include "executor.h"
#include "compile.h"
#include <iostream>
#include <memory>
#include <string_view>
#include <atomic>
#include <mutex>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>

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
		
		context->currentModule.push("__main__");
		context->globals.insert({ std::string("__main__"), {} });

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

	static Wg_Obj* Compile(Wg_Context* context, const char* code, const char* module, const char* prettyName, bool expr) {
		WASSERT(context && code);

		if (prettyName == nullptr)
			prettyName = wings::DEFAULT_FUNC_NAME;

		auto lexResult = wings::Lex(code);
		auto originalSource = wings::MakeRcPtr<std::vector<std::string>>(lexResult.originalSource);

		auto raiseException = [&](const wings::CodeError& error) {
			context->currentTrace.push_back(wings::TraceFrame{
				error.srcPos,
				error.srcPos.line < originalSource->size() ? (*originalSource)[error.srcPos.line] : "",
				module,
				prettyName
				});

			Wg_RaiseException(context, WG_EXC_SYNTAXERROR, error.message.c_str());
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

		if (expr) {
			std::vector<wings::Statement> body = std::move(parseResult.parseTree.expr.def.body);
			if (body.size() != 1 || body[0].type != wings::Statement::Type::Expr) {
				raiseException(wings::CodeError::Bad("Invalid syntax"));
				return nullptr;
			}
			
			wings::Statement ret{};
			ret.srcPos = body[0].srcPos;
			ret.type = wings::Statement::Type::Return;
			ret.expr = std::move(body[0].expr);
			
			parseResult.parseTree.expr.def.body.clear();
			parseResult.parseTree.expr.def.body.push_back(std::move(ret));
		}

		auto* def = new wings::DefObject();
		def->context = context;
		def->module = module;
		def->prettyName = prettyName;
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

	Wg_Obj* Wg_Compile(Wg_Context* context, const char* code, const char* prettyName) {
		return Compile(context, code, "__main__", prettyName, false);
	}

	Wg_Obj* Wg_CompileExpression(Wg_Context* context, const char* code, const char* prettyName) {
		return Compile(context, code, "__main__", prettyName, true);
	}

	Wg_Obj* Wg_GetGlobal(Wg_Context* context, const char* name) {
		WASSERT(context && name);
		auto module = std::string(context->currentModule.top());
		auto& globals = context->globals.at(module);
		auto it = globals.find(name);
		if (it == globals.end()) {
			return nullptr;
		} else {
			return *it->second;
		}
	}

	void Wg_SetGlobal(Wg_Context* context, const char* name, Wg_Obj* value) {
		WASSERT_VOID(context && name && value);
		const auto& module = std::string(context->currentModule.top());
		auto& globals = context->globals.at(module);
		auto it = globals.find(name);
		if (it != globals.end()) {
			*it->second = value;
		} else {
			globals.insert({ std::string(name), wings::MakeRcPtr<Wg_Obj*>(value) });
		}
	}

	void Wg_DeleteGlobal(Wg_Context* context, const char* name) {
		WASSERT_VOID(context && name);
		const auto& module = std::string(context->currentModule.top());
		auto& globals = context->globals.at(module);
		auto it = globals.find(name);
		if (it != globals.end()) {
			globals.erase(it);
		}
	}


	void Wg_RegisterModule(Wg_Context* context, const char* name, Wg_ModuleLoader loader) {
		context->moduleLoaders.insert({ std::string(name), loader });
	}

	static bool ReadFromFile(const std::string& path, std::string& data) {
		std::ifstream f(path);
		if (!f.is_open())
			return false;

		std::stringstream buffer;
		buffer << f.rdbuf();

		if (!f)
			return false;

		data = buffer.str();
		return true;
	}

	static bool LoadFileModule(Wg_Context* context, const std::string& module) {
		std::string path = context->importPath + module + ".py";
		std::string source;
		if (!ReadFromFile(path, source)) {
			std::string msg = std::string("No module named '") + module + "'";
			Wg_RaiseException(context, WG_EXC_IMPORTERROR, msg.c_str());
			return false;
		}

		Wg_Obj* fn = Compile(context, source.c_str(), module.c_str(), module.c_str(), false);
		if (fn == nullptr)
			return false;

		return Wg_Call(fn, nullptr, 0) != nullptr;
	}

	static bool LoadModule(Wg_Context* context, const std::string& name) {
		if (!context->globals.contains(name)) {
			bool success{};
			context->globals.insert({ std::string(name), {} });
			context->currentModule.push(name);

			auto it = context->moduleLoaders.find(name);
			if (it != context->moduleLoaders.end()) {
				success = it->second(context);
			} else {
				success = LoadFileModule(context, name);
			}

			context->currentModule.pop();
			if (!success) {
				context->globals.erase(name);
				return false;
			}
		}
		return true;
	}

	Wg_Obj* Wg_ImportModule(Wg_Context* context, const char* module, const char* alias) {
		WASSERT(context && module);

		if (!LoadModule(context, module))
			return nullptr;

		Wg_Obj* moduleObject = Wg_Call(context->builtins.moduleObject, nullptr, 0);
		if (moduleObject == nullptr)
			return nullptr;
		auto& mod = context->globals.at(module);
		for (auto& [var, val] : mod) {
			Wg_SetAttribute(moduleObject, var.c_str(), *val);
		}
		Wg_SetGlobal(context, alias ? alias : module, moduleObject);
		return moduleObject;
	}

	Wg_Obj* Wg_ImportFromModule(Wg_Context* context, const char* module, const char* name, const char* alias) {
		WASSERT(context && module && name);

		if (!LoadModule(context, module))
			return nullptr;

		auto& mod = context->globals.at(module);
		auto it = mod.find(name);
		if (it == mod.end()) {
			std::string msg = std::string("Cannot import '") + name
				+ "' from '" + module + "'";
			Wg_RaiseException(context, WG_EXC_IMPORTERROR, msg.c_str());
			return nullptr;
		}

		Wg_SetGlobal(context, alias ? alias : name, *it->second);
		return *it->second;
	}

	bool Wg_ImportAllFromModule(Wg_Context* context, const char* module) {
		WASSERT(context && module);

		if (!LoadModule(context, module))
			return false;

		auto& mod = context->globals.at(module);
		for (auto& [var, val] : mod) {
			Wg_SetGlobal(context, var.c_str(), *val);
		}
		return true;
	}

	void Wg_SetImportPath(Wg_Context* context, const char* path) {
		context->importPath = path;
		if (path[0] != '/' && path[0] != '\\')
			context->importPath += "/";
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
