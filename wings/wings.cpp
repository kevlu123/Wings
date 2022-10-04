#include "wings.h"
#include "common.h"
#include "executor.h"
#include "compile.h"
#include "lib.h"

#include <iostream>
#include <memory>
#include <fstream>
#include <vector>
#include <string>
#include <string_view>
#include <sstream>
#include <queue>
#include <mutex>

extern "C" {
	void Wg_GetConfig(const Wg_Context* context, Wg_Config* out) {
		WG_ASSERT_VOID(context && out);
		*out = context->config;
	}

	void Wg_SetConfig(Wg_Context* context, const Wg_Config* config) {
		WG_ASSERT_VOID(context);
		if (config) {
			WG_ASSERT_VOID(config->maxAlloc >= 0);
			WG_ASSERT_VOID(config->maxRecursion >= 0);
			WG_ASSERT_VOID(config->maxCollectionSize >= 0);
			WG_ASSERT_VOID(config->gcRunFactor >= 1.0f);
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

		Wg_RegisterModule(context, "__builtins__", wings::LoadBuiltins);
		Wg_ImportAllFromModule(context, "__builtins__");
		
		// Apply possibly restrictive config now
		Wg_SetConfig(context, config);
		
		return context;
	}

	void Wg_DestroyContext(Wg_Context* context) {
		WG_ASSERT_VOID(context);
		wings::DestroyAllObjects(context);
		delete context;
	}

	void Wg_Print(const Wg_Context* context, const char* message, int len) {
		WG_ASSERT_VOID(context && message);
		if (context->config.print) {
			context->config.print(len ? message : "", len, context->config.printUserdata);
		}
	}

	void Wg_PrintString(const Wg_Context* context, const char* message) {
		WG_ASSERT_VOID(context && message);
		Wg_Print(context, message, (int)std::strlen(message));
	}

	void Wg_SetErrorCallback(Wg_ErrorCallback callback, void* userdata) {
		std::scoped_lock lock(wings::errorCallbackMutex);
		wings::errorCallback = callback;
		wings::errorCallbackUserdata = userdata;
	}

	static Wg_Obj* Compile(Wg_Context* context, const char* code, const char* module, const char* prettyName, bool expr) {
		WG_ASSERT(context && code);

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
		WG_ASSERT(context && name && wings::IsValidIdentifier(name));
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
		WG_ASSERT_VOID(context && name && value && wings::IsValidIdentifier(name));
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
		WG_ASSERT_VOID(context && name && wings::IsValidIdentifier(name));
		const auto& module = std::string(context->currentModule.top());
		auto& globals = context->globals.at(module);
		auto it = globals.find(name);
		if (it != globals.end()) {
			globals.erase(it);
		}
	}

	void Wg_RegisterModule(Wg_Context* context, const char* name, Wg_ModuleLoader loader) {
		WG_ASSERT_VOID(context && name && loader && wings::IsValidIdentifier(name));
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
		WG_ASSERT(context && module && wings::IsValidIdentifier(module));
		if (alias) {
			WG_ASSERT(wings::IsValidIdentifier(alias));
		} else {
			alias = module;
		}

		if (!LoadModule(context, module))
			return nullptr;

		Wg_Obj* moduleObject = Wg_Call(context->builtins.moduleObject, nullptr, 0);
		if (moduleObject == nullptr)
			return nullptr;
		auto& mod = context->globals.at(module);
		for (auto& [var, val] : mod) {
			Wg_SetAttribute(moduleObject, var.c_str(), *val);
		}
		Wg_SetGlobal(context, alias, moduleObject);
		return moduleObject;
	}

	Wg_Obj* Wg_ImportFromModule(Wg_Context* context, const char* module, const char* name, const char* alias) {
		WG_ASSERT(context && module && name && wings::IsValidIdentifier(module));
		if (alias) {
			WG_ASSERT(wings::IsValidIdentifier(alias));
		} else {
			alias = name;
		}

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

		Wg_SetGlobal(context, alias, *it->second);
		return *it->second;
	}

	bool Wg_ImportAllFromModule(Wg_Context* context, const char* module) {
		WG_ASSERT(context && module && wings::IsValidIdentifier(module));

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

	Wg_Obj* Wg_CreateNone(Wg_Context* context) {
		WG_ASSERT(context);
		return context->builtins.none;
	}

	Wg_Obj* Wg_CreateBool(Wg_Context* context, bool value) {
		WG_ASSERT(context);
		if (value && context->builtins._true) {
			return context->builtins._true;
		} else if (!value && context->builtins._false) {
			return context->builtins._false;
		} else {
			return value ? context->builtins._true : context->builtins._false;
		}
	}

	Wg_Obj* Wg_CreateInt(Wg_Context* context, Wg_int value) {
		WG_ASSERT(context);
		if (Wg_Obj* v = Wg_Call(context->builtins._int, nullptr, 0)) {
			v->Get<Wg_int>() = value;
			return v;
		} else {
			return nullptr;
		}
	}

	Wg_Obj* Wg_CreateFloat(Wg_Context* context, Wg_float value) {
		WG_ASSERT(context);
		if (Wg_Obj* v = Wg_Call(context->builtins._float, nullptr, 0)) {
			v->Get<Wg_float>() = value;
			return v;
		} else {
			return nullptr;
		}
	}

	Wg_Obj* Wg_CreateString(Wg_Context* context, const char* value) {
		WG_ASSERT(context);
		if (Wg_Obj* v = Wg_Call(context->builtins.str, nullptr, 0)) {
			v->Get<std::string>() = value ? value : "";
			return v;
		} else {
			return nullptr;
		}
	}

	Wg_Obj* Wg_CreateTuple(Wg_Context* context, Wg_Obj** argv, int argc) {
		std::vector<wings::Wg_ObjRef> refs;
		WG_ASSERT(context && argc >= 0);
		if (argc > 0) {
			WG_ASSERT(argv);
			for (int i = 0; i < argc; i++) {
				refs.emplace_back(argv[i]);
				WG_ASSERT(argv[i]);
			}
		}

		if (Wg_Obj* v = Wg_Call(context->builtins.tuple, nullptr, 0)) {
			v->Get<std::vector<Wg_Obj*>>() = std::vector<Wg_Obj*>(argv, argv + argc);
			return v;
		} else {
			return nullptr;
		}
	}

	Wg_Obj* Wg_CreateList(Wg_Context* context, Wg_Obj** argv, int argc) {
		std::vector<wings::Wg_ObjRef> refs;
		WG_ASSERT(context && argc >= 0);
		if (argc > 0) {
			WG_ASSERT(argv);
			for (int i = 0; i < argc; i++) {
				refs.emplace_back(argv[i]);
				WG_ASSERT(argv[i]);
			}
		}

		if (Wg_Obj* v = Wg_Call(context->builtins.list, nullptr, 0)) {
			v->Get<std::vector<Wg_Obj*>>() = std::vector<Wg_Obj*>(argv, argv + argc);
			return v;
		} else {
			return nullptr;
		}
	}

	Wg_Obj* Wg_CreateDictionary(Wg_Context* context, Wg_Obj** keys, Wg_Obj** values, int argc) {
		std::vector<wings::Wg_ObjRef> refs;
		WG_ASSERT(context && argc >= 0);
		if (argc > 0) {
			WG_ASSERT(keys && values);
			for (int i = 0; i < argc; i++) {
				refs.emplace_back(keys[i]);
				refs.emplace_back(values[i]);
				WG_ASSERT(keys[i] && values[i]);
			}
		}

		// Pass a dummy kwargs to prevent stack overflow from recursion
		Wg_Obj* dummyKwargs = wings::Alloc(context);
		if (dummyKwargs == nullptr)
			return nullptr;
		dummyKwargs->type = "__map";
		wings::WDict wd{};
		dummyKwargs->data = &wd;

		if (Wg_Obj* v = Wg_Call(context->builtins.dict, nullptr, 0, dummyKwargs)) {
			for (int i = 0; i < argc; i++) {
				refs.emplace_back(v);
				try {
					v->Get<wings::WDict>()[keys[i]] = values[i];
				} catch (wings::HashException&) {
					return nullptr;
				}
			}
			return v;
		} else {
			return nullptr;
		}
	}

	Wg_Obj* Wg_CreateSet(Wg_Context* context, Wg_Obj** argv, int argc) {
		std::vector<wings::Wg_ObjRef> refs;
		WG_ASSERT(context && argc >= 0);
		if (argc > 0) {
			WG_ASSERT(argv);
			for (int i = 0; i < argc; i++) {
				refs.emplace_back(argv[i]);
				WG_ASSERT(argv[i]);
			}
		}

		if (Wg_Obj* v = Wg_Call(context->builtins.set, nullptr, 0, nullptr)) {
			for (int i = 0; i < argc; i++) {
				try {
					v->Get<wings::WSet>().insert(argv[i]);
				} catch (wings::HashException&) {
					return nullptr;
				}
			}
			return v;
		} else {
			return nullptr;
		}
	}

	Wg_Obj* Wg_CreateFunction(Wg_Context* context, const Wg_FuncDesc* value) {
		WG_ASSERT(context && value);
		if (Wg_Obj* v = Wg_Call(context->builtins.func, nullptr, 0)) {
			v->Get<Wg_Obj::Func>() = {
				nullptr,
				value->fptr,
				value->userdata,
				value->isMethod,
				std::string(context->currentModule.top()),
				std::string(value->prettyName ? value->prettyName : wings::DEFAULT_FUNC_NAME)
			};
			return v;
		} else {
			return nullptr;
		}
	}

	Wg_Obj* Wg_CreateClass(Wg_Context* context, const char* name, Wg_Obj** bases, int baseCount) {
		std::vector<wings::Wg_ObjRef> refs;
		WG_ASSERT(context && name && baseCount >= 0);
		if (baseCount > 0) {
			WG_ASSERT(bases);
			for (int i = 0; i < baseCount; i++) {
				WG_ASSERT(bases[i] && Wg_IsClass(bases[i]));
				refs.emplace_back(bases[i]);
			}
		}

		// Allocate class
		Wg_Obj* _class = wings::Alloc(context);
		if (_class == nullptr) {
			return nullptr;
		}
		refs.emplace_back(_class);
		_class->type = "__class";
		_class->data = new Wg_Obj::Class{ std::string(name) };
		_class->finalizer.fptr = [](Wg_Obj* obj, void*) { delete (Wg_Obj::Class*)obj->data; };
		_class->Get<Wg_Obj::Class>().module = context->currentModule.top();
		_class->Get<Wg_Obj::Class>().instanceAttributes.Set("__class__", _class);
		_class->attributes.AddParent(context->builtins.object->Get<Wg_Obj::Class>().instanceAttributes);

		// Set bases
		int actualBaseCount = baseCount ? baseCount : 1;
		Wg_Obj** actualBases = baseCount ? bases : &context->builtins.object;
		for (int i = 0; i < actualBaseCount; i++) {
			_class->Get<Wg_Obj::Class>().instanceAttributes.AddParent(actualBases[i]->Get<Wg_Obj::Class>().instanceAttributes);
			_class->Get<Wg_Obj::Class>().bases.push_back(actualBases[i]);
		}
		if (Wg_Obj* basesTuple = Wg_CreateTuple(context, actualBases, actualBaseCount)) {
			_class->attributes.Set("__bases__", basesTuple);
		} else {
			return nullptr;
		}

		// Set __str__()
		Wg_FuncDesc tostr{};
		tostr.isMethod = true;
		tostr.prettyName = "__str__";
		tostr.userdata = context;
		tostr.fptr = [](Wg_Context* context, Wg_Obj** argv, int argc) -> Wg_Obj* {
			if (argc != 1) {
				Wg_RaiseArgumentCountError(context, argc, 1);
				return nullptr;
			}
			std::string s = "<class '" + argv[0]->Get<Wg_Obj::Class>().name + "'>";
			return Wg_CreateString(argv[0]->context, s.c_str());
		};
		if (Wg_Obj* tostrFn = Wg_CreateFunction(context, &tostr)) {
			Wg_SetAttribute(_class, "__str__", tostrFn);
		} else {
			return nullptr;
		}

		// Set construction function. This function forwards to __init__().
		_class->Get<Wg_Obj::Class>().userdata = _class;
		_class->Get<Wg_Obj::Class>().ctor = [](Wg_Context* context, Wg_Obj** argv, int argc) -> Wg_Obj* {
			Wg_Obj* _classObj = (Wg_Obj*)Wg_GetFunctionUserdata(context);

			Wg_Obj* instance = wings::Alloc(context);
			if (instance == nullptr)
				return nullptr;
			wings::Wg_ObjRef ref(instance);

			instance->attributes = _classObj->Get<Wg_Obj::Class>().instanceAttributes.Copy();
			instance->type = _classObj->Get<Wg_Obj::Class>().name;

			if (Wg_Obj* init = Wg_HasAttribute(instance, "__init__")) {
				if (Wg_IsFunction(init)) {
					Wg_Obj* kwargs = Wg_GetKwargs(context);
					if (kwargs == nullptr)
						return nullptr;

					Wg_Obj* ret = Wg_Call(init, argv, argc, kwargs);
					if (ret == nullptr) {
						return nullptr;
					} else if (!Wg_IsNone(ret)) {
						Wg_RaiseException(context, WG_EXC_TYPEERROR, "__init__() returned a non NoneType type");
						return nullptr;
					}
				}
			}

			return instance;
		};

		// Set init method
		std::string initName = std::string(name) + ".__init__";
		Wg_FuncDesc init{};
		init.prettyName = initName.c_str();
		init.isMethod = true;
		init.userdata = _class;
		init.fptr = [](Wg_Context* context, Wg_Obj** argv, int argc) -> Wg_Obj* {
			Wg_Obj* _class = (Wg_Obj*)Wg_GetFunctionUserdata(context);
			if (argc < 1) {
				Wg_RaiseArgumentCountError(_class->context, argc, -1);
				return nullptr;
			}

			const auto& bases = _class->Get<Wg_Obj::Class>().bases;
			if (bases.empty())
				return nullptr;

			if (Wg_Obj* baseInit = Wg_GetAttributeFromBase(argv[0], "__init__", bases[0])) {
				Wg_Obj* kwargs = Wg_GetKwargs(context);
				if (kwargs == nullptr)
					return nullptr;

				Wg_Obj* ret = Wg_Call(baseInit, argv + 1, argc - 1, kwargs);
				if (ret == nullptr) {
					return nullptr;
				} else if (!Wg_IsNone(ret)) {
					Wg_RaiseException(context, WG_EXC_TYPEERROR, "__init__() returned a non NoneType type");
					return nullptr;
				}
			}

			return Wg_CreateNone(context);
		};
		Wg_Obj* initFn = Wg_CreateFunction(context, &init);
		if (initFn == nullptr)
			return nullptr;
		Wg_LinkReference(initFn, _class);
		Wg_AddAttributeToClass(_class, "__init__", initFn);

		return _class;
	}

	void Wg_AddAttributeToClass(Wg_Obj* class_, const char* attribute, Wg_Obj* value) {
		WG_ASSERT_VOID(class_ && attribute && value && Wg_IsClass(class_) && wings::IsValidIdentifier(attribute));
		class_->Get<Wg_Obj::Class>().instanceAttributes.Set(attribute, value);
	}

	bool Wg_IsNone(const Wg_Obj* obj) {
		WG_ASSERT(obj);
		return obj == obj->context->builtins.none;
	}

	bool Wg_IsBool(const Wg_Obj* obj) {
		WG_ASSERT(obj);
		return obj == obj->context->builtins._true
			|| obj == obj->context->builtins._false;
	}

	bool Wg_IsInt(const Wg_Obj* obj) {
		WG_ASSERT(obj);
		return obj->type == "__int";
	}

	bool Wg_IsIntOrFloat(const Wg_Obj* obj) {
		WG_ASSERT(obj);
		return obj->type == "__int" || obj->type == "__float";
	}

	bool Wg_IsString(const Wg_Obj* obj) {
		WG_ASSERT(obj);
		return obj->type == "__str";
	}

	bool Wg_IsTuple(const Wg_Obj* obj) {
		WG_ASSERT(obj);
		return obj->type == "__tuple";
	}

	bool Wg_IsList(const Wg_Obj* obj) {
		WG_ASSERT(obj);
		return obj->type == "__list";
	}

	bool Wg_IsDictionary(const Wg_Obj* obj) {
		WG_ASSERT(obj);
		return obj->type == "__map";
	}

	bool Wg_IsSet(const Wg_Obj* obj) {
		WG_ASSERT(obj);
		return obj->type == "__set";
	}

	bool Wg_IsClass(const Wg_Obj* obj) {
		WG_ASSERT(obj);
		return obj->type == "__class";
	}

	bool Wg_IsFunction(const Wg_Obj* obj) {
		WG_ASSERT(obj);
		return obj->type == "__func";
	}

	bool Wg_GetBool(const Wg_Obj* obj) {
		WG_ASSERT(obj && Wg_IsBool(obj));
		return obj->Get<bool>();
	}

	Wg_int Wg_GetInt(const Wg_Obj* obj) {
		WG_ASSERT(obj && Wg_IsInt(obj));
		return obj->Get<Wg_int>();
	}

	Wg_float Wg_GetFloat(const Wg_Obj* obj) {
		WG_ASSERT(obj && Wg_IsIntOrFloat(obj));
		if (Wg_IsInt(obj)) return (Wg_float)obj->Get<Wg_int>();
		else return obj->Get<Wg_float>();
	}

	const char* Wg_GetString(const Wg_Obj* obj) {
		WG_ASSERT(obj && Wg_IsString(obj));
		return obj->Get<std::string>().c_str();
	}

	void Wg_SetUserdata(Wg_Obj* obj, void* userdata) {
		WG_ASSERT_VOID(obj);
		obj->data = userdata;
	}

	bool Wg_TryGetUserdata(const Wg_Obj* obj, const char* type, void** out) {
		WG_ASSERT(obj && type);
		if (obj->type == std::string(type)) {
			*out = obj->data;
			return true;
		} else {
			return false;
		}
	}

	void Wg_GetFinalizer(const Wg_Obj* obj, Wg_FinalizerDesc* out) {
		WG_ASSERT_VOID(obj && out);
		*out = obj->finalizer;
	}

	void Wg_SetFinalizer(Wg_Obj* obj, const Wg_FinalizerDesc* finalizer) {
		WG_ASSERT_VOID(obj && finalizer);
		obj->finalizer = *finalizer;
	}

	Wg_Obj* Wg_HasAttribute(Wg_Obj* obj, const char* member) {
		WG_ASSERT(obj && member && wings::IsValidIdentifier(member));
		Wg_Obj* mem = obj->attributes.Get(member);
		if (mem && Wg_IsFunction(mem) && mem->Get<Wg_Obj::Func>().isMethod) {
			mem->Get<Wg_Obj::Func>().self = obj;
		}
		return mem;
	}

	bool Wg_DeleteAttribute(Wg_Obj* obj, const char* member) {
		std::abort(); // TODO
		//WG_ASSERT(obj && member && wings::IsValidIdentifier(member));
		//Wg_Obj* mem = obj->attributes.Delete(member);
		//if (mem == nullptr) {
		//	Wg_RaiseAttributeError(obj, member);
		//} else if (Wg_IsFunction(mem) && mem->Get<Wg_Obj::Func>().isMethod) {
		//	mem->Get<Wg_Obj::Func>().self = obj;
		//}
		//return mem;
	}

	Wg_Obj* Wg_GetAttribute(Wg_Obj* obj, const char* member) {
		WG_ASSERT(obj && member && wings::IsValidIdentifier(member));
		Wg_Obj* mem = obj->attributes.Get(member);
		if (mem == nullptr) {
			Wg_RaiseAttributeError(obj, member);
		} else if (Wg_IsFunction(mem) && mem->Get<Wg_Obj::Func>().isMethod) {
			mem->Get<Wg_Obj::Func>().self = obj;
		}
		return mem;
	}

	void Wg_SetAttribute(Wg_Obj* obj, const char* member, Wg_Obj* value) {
		WG_ASSERT_VOID(obj && member && value && wings::IsValidIdentifier(member));
		obj->attributes.Set(member, value);
	}

	Wg_Obj* Wg_GetAttributeFromBase(Wg_Obj* obj, const char* member, Wg_Obj* baseClass) {
		WG_ASSERT(obj && member && wings::IsValidIdentifier(member));

		Wg_Obj* mem{};
		if (baseClass == nullptr) {
			mem = obj->attributes.GetFromBase(member);
		} else {
			mem = baseClass->Get<Wg_Obj::Class>().instanceAttributes.Get(member);
		}

		if (mem && Wg_IsFunction(mem) && mem->Get<Wg_Obj::Func>().isMethod) {
			mem->Get<Wg_Obj::Func>().self = obj;
		}
		return mem;
	}

	Wg_Obj* Wg_IsInstance(const Wg_Obj* instance, Wg_Obj* const* types, int typesLen) {
		WG_ASSERT(instance && typesLen >= 0 && (types || typesLen == 0));
		for (int i = 0; i < typesLen; i++)
			WG_ASSERT(types[i] && Wg_IsClass(types[i]));

		// Cannot use Wg_HasAttribute here because instance is a const pointer
		Wg_Obj* _class = instance->attributes.Get("__class__");
		if (_class == nullptr)
			return nullptr;
		wings::Wg_ObjRef ref(_class);

		std::queue<wings::Wg_ObjRef> toCheck;
		toCheck.emplace(_class);

		while (!toCheck.empty()) {
			auto end = types + typesLen;
			auto it = std::find(types, end, toCheck.front().Get());
			if (it != end)
				return *it;

			Wg_Obj* bases = Wg_HasAttribute(toCheck.front().Get(), "__bases__");
			if (bases && Wg_IsTuple(bases))
				for (Wg_Obj* base : bases->Get<std::vector<Wg_Obj*>>())
					toCheck.emplace(base);

			toCheck.pop();
		}
		return nullptr;
	}

	bool Wg_Iterate(Wg_Obj* obj, void* userdata, Wg_IterationCallback callback) {
		WG_ASSERT(obj && callback);
		Wg_Context* context = obj->context;

		Wg_Obj* iter = Wg_CallMethod(obj, "__iter__", nullptr, 0);
		if (iter == nullptr)
			return false;
		wings::Wg_ObjRef iterRef(iter);

		while (true) {
			Wg_Obj* yielded = Wg_CallMethod(iter, "__next__", nullptr, 0);

			Wg_Obj* exc = Wg_GetCurrentException(context);
			if (exc) {
				if (Wg_IsInstance(exc, &context->builtins.stopIteration, 1)) {
					Wg_ClearCurrentException(context);
					return true;
				} else {
					return false;
				}
			}

			WG_ASSERT(yielded); // If no exception was thrown then a value must be yielded
			wings::Wg_ObjRef yieldedRef(yielded);
			if (!callback(yielded, userdata))
				return Wg_GetCurrentException(context) == nullptr;
		}
	}

	bool Wg_Unpack(Wg_Obj* obj, Wg_Obj** out, int count) {
		WG_ASSERT(obj && (count == 0 || out));

		Wg_Context* context = obj->context;
		struct State {
			Wg_Context* context;
			Wg_Obj** array;
			int count;
			int index;
		} s = { context, out, count, 0 };

		bool success = Wg_Iterate(obj, &s, [](Wg_Obj* yielded, void* userdata) {
			State* s = (State*)userdata;
			if (s->index >= s->count) {
				Wg_RaiseException(s->context, WG_EXC_VALUEERROR, "Too many values to unpack");
			} else {
				Wg_ProtectObject(yielded);
				s->array[s->index] = yielded;
				s->index++;
			}
			return true;
			});

		for (int i = s.index; i; i--)
			Wg_UnprotectObject(out[i - 1]);

		if (!success) {
			return false;
		} else if (s.index < count) {
			Wg_RaiseException(context, WG_EXC_VALUEERROR, "Not enough values to unpack");
			return false;
		} else {
			return true;
		}
	}

	Wg_Obj* Wg_GetKwargs(Wg_Context* context) {
		WG_ASSERT(context && !context->kwargs.empty());
		if (context->kwargs.back() == nullptr) {
			context->kwargs.back() = Wg_CreateDictionary(context);
		}
		return context->kwargs.back();
	}

	void* Wg_GetFunctionUserdata(Wg_Context* context) {
		WG_ASSERT(context && !context->kwargs.empty());
		return context->userdata.back();
	}

	Wg_Obj* Wg_Call(Wg_Obj* callable, Wg_Obj** argv, int argc, Wg_Obj* kwargsDict) {
		WG_ASSERT(callable && argc >= 0 && (argc == 0 || argv));
		if (Wg_IsFunction(callable) || Wg_IsClass(callable)) {
			if (argc)
				WG_ASSERT(argv);
			for (int i = 0; i < argc; i++)
				WG_ASSERT(argv[i]);

			Wg_Context* context = callable->context;

			if (kwargsDict) {
				if (!Wg_IsDictionary(kwargsDict)) {
					Wg_RaiseException(context, WG_EXC_TYPEERROR, "Keyword arguments must be a dictionary");
					return nullptr;
				}
				for (const auto& [key, value] : kwargsDict->Get<wings::WDict>()) {
					if (!Wg_IsString(key)) {
						Wg_RaiseException(context, WG_EXC_TYPEERROR, "Keyword arguments dictionary must only contain string keys");
						return nullptr;
					}
				}
			}

			std::vector<wings::Wg_ObjRef> refs;
			refs.emplace_back(callable);
			for (int i = 0; i < argc; i++)
				refs.emplace_back(argv[i]);

			Wg_Obj* (*fptr)(Wg_Context*, Wg_Obj**, int);
			void* userdata = nullptr;
			Wg_Obj* self = nullptr;
			std::string_view module;
			if (Wg_IsFunction(callable)) {
				const auto& func = callable->Get<Wg_Obj::Func>();
				if (func.self)
					self = func.self;
				fptr = func.fptr;
				userdata = func.userdata;
				module = func.module;

				context->currentTrace.push_back(wings::TraceFrame{
					{},
					"",
					func.module,
					func.prettyName
					});
			} else {
				fptr = callable->Get<Wg_Obj::Class>().ctor;
				userdata = callable->Get<Wg_Obj::Class>().userdata;
				module = callable->Get<Wg_Obj::Class>().module;
			}

			std::vector<Wg_Obj*> argsWithSelf;
			if (self) {
				argsWithSelf.push_back(self);
				refs.emplace_back(self);
			}
			argsWithSelf.insert(argsWithSelf.end(), argv, argv + argc);

			context->currentModule.push(module);
			context->userdata.push_back(userdata);
			context->kwargs.push_back(kwargsDict);
			Wg_Obj* ret = fptr(context, argsWithSelf.data(), (int)argsWithSelf.size());
			context->kwargs.pop_back();
			context->userdata.pop_back();
			context->currentModule.pop();

			if (Wg_IsFunction(callable)) {
				context->currentTrace.pop_back();
			}

			return ret;
		} else {
			return Wg_CallMethod(callable, "__call__", argv, argc);
		}
	}

	Wg_Obj* Wg_CallMethod(Wg_Obj* obj, const char* member, Wg_Obj** argv, int argc, Wg_Obj* kwargsDict) {
		WG_ASSERT(obj && member && wings::IsValidIdentifier(member));
		if (argc)
			WG_ASSERT(argv);
		for (int i = 0; i < argc; i++)
			WG_ASSERT(argv[i]);

		if (Wg_Obj* method = Wg_GetAttribute(obj, member)) {
			return Wg_Call(method, argv, argc, kwargsDict);
		} else {
			return nullptr;
		}
	}

	Wg_Obj* Wg_CallMethodFromBase(Wg_Obj* obj, const char* member, Wg_Obj** argv, int argc, Wg_Obj* kwargsDict, Wg_Obj* baseClass) {
		WG_ASSERT(obj && member && wings::IsValidIdentifier(member));
		if (argc)
			WG_ASSERT(argv);
		for (int i = 0; i < argc; i++)
			WG_ASSERT(argv[i]);

		if (Wg_Obj* method = Wg_GetAttributeFromBase(obj, member, baseClass)) {
			return Wg_Call(method, argv, argc, kwargsDict);
		} else {
			Wg_RaiseAttributeError(obj, member);
			return nullptr;
		}
	}

	bool Wg_ParseKwargs(Wg_Obj* kwargs, const char* const* keys, int count, Wg_Obj** out) {
		WG_ASSERT(kwargs && keys && out && count > 0 && Wg_IsDictionary(kwargs));

		wings::Wg_ObjRef ref(kwargs);
		auto& buf = kwargs->Get<wings::WDict>();
		for (int i = 0; i < count; i++) {
			Wg_Obj* key = Wg_CreateString(kwargs->context, keys[i]);
			if (key == nullptr)
				return false;

			wings::WDict::iterator it;
			try {
				it = buf.find(key);
			} catch (wings::HashException&) {
				return false;
			}

			if (it != buf.end()) {
				out[i] = it->second;
			} else {
				out[i] = nullptr;
			}
		}
		return true;
	}

	Wg_Obj* Wg_GetIndex(Wg_Obj* obj, Wg_Obj* index) {
		WG_ASSERT(obj && index);
		return Wg_CallMethod(obj, "__getitem__", &index, 1);
	}

	Wg_Obj* Wg_SetIndex(Wg_Obj* obj, Wg_Obj* index, Wg_Obj* value) {
		WG_ASSERT(obj && index && value);
		Wg_Obj* argv[2] = { index, value };
		return Wg_CallMethod(obj, "__setitem__", argv, 2);
	}

	Wg_Obj* Wg_UnaryOp(Wg_UnOp op, Wg_Obj* arg) {
		WG_ASSERT(arg);
		Wg_Context* context = arg->context;
		switch (op) {
		case WG_UOP_POS:
			return Wg_CallMethod(arg, "__pos__", nullptr, 0);
		case WG_UOP_NEG:
			return Wg_CallMethod(arg, "__neg__", nullptr, 0);
		case WG_UOP_BITNOT:
			return Wg_CallMethod(arg, "__invert__", nullptr, 0);
		case WG_UOP_HASH:
			return Wg_Call(context->builtins.hash, &arg, 1);
		case WG_UOP_LEN:
			return Wg_Call(context->builtins.len, &arg, 1);
		case WG_UOP_BOOL:
			return Wg_Call(context->builtins._bool, &arg, 1);
		case WG_UOP_INT:
			return Wg_Call(context->builtins._int, &arg, 1);
		case WG_UOP_FLOAT:
			return Wg_Call(context->builtins._float, &arg, 1);
		case WG_UOP_STR:
			return Wg_Call(context->builtins.str, &arg, 1);
		case WG_UOP_REPR:
			return Wg_Call(context->builtins.repr, &arg, 1);
		case WG_UOP_INDEX: {
			Wg_Obj* index = Wg_CallMethod(arg, "__index__", nullptr, 0);
			if (index == nullptr) {
				return nullptr;
			} else if (!Wg_IsInt(index)) {
				Wg_RaiseException(context, WG_EXC_TYPEERROR, "__index__() returned a non integer type");
				return nullptr;
			} else {
				return index;
			}
		}
		default:
			WG_UNREACHABLE();
		}
	}

	static const std::unordered_map<Wg_BinOp, const char*> OP_METHOD_NAMES = {
		{ WG_BOP_ADD, "__add__" },
		{ WG_BOP_SUB, "__sub__" },
		{ WG_BOP_MUL, "__mul__" },
		{ WG_BOP_DIV, "__truediv__" },
		{ WG_BOP_FLOORDIV, "__floordiv__" },
		{ WG_BOP_MOD, "__mod__" },
		{ WG_BOP_POW, "__pow__" },
		{ WG_BOP_BITAND, "__and__" },
		{ WG_BOP_BITOR, "__or__" },
		{ WG_BOP_BITXOR, "__not__" },
		{ WG_BOP_SHL, "__lshift__" },
		{ WG_BOP_SHR, "__rshift__" },
		{ WG_BOP_IN, "__contains__" },
		{ WG_BOP_EQ, "__eq__" },
		{ WG_BOP_NE, "__ne__" },
		{ WG_BOP_LT, "__lt__" },
		{ WG_BOP_LE, "__le__" },
		{ WG_BOP_GT, "__gt__" },
		{ WG_BOP_GE, "__ge__" },
	};

	Wg_Obj* Wg_BinaryOp(Wg_BinOp op, Wg_Obj* lhs, Wg_Obj* rhs) {
		WG_ASSERT(lhs && rhs);

		if (op == WG_BOP_IN)
			std::swap(lhs, rhs);

		auto method = OP_METHOD_NAMES.find(op);

		switch (op) {
		case WG_BOP_ADD:
		case WG_BOP_SUB:
		case WG_BOP_MUL:
		case WG_BOP_DIV:
		case WG_BOP_FLOORDIV:
		case WG_BOP_MOD:
		case WG_BOP_POW:
		case WG_BOP_BITAND:
		case WG_BOP_BITOR:
		case WG_BOP_BITXOR:
		case WG_BOP_SHL:
		case WG_BOP_SHR:
			return Wg_CallMethod(lhs, method->second, &rhs, 1);
		case WG_BOP_EQ:
		case WG_BOP_NE:
		case WG_BOP_LT:
		case WG_BOP_LE:
		case WG_BOP_GT:
		case WG_BOP_GE:
		case WG_BOP_IN: {
			Wg_Obj* boolResult = Wg_CallMethod(lhs, method->second, &rhs, 1);
			if (!Wg_IsBool(boolResult)) {
				std::string message = method->second;
				message += "() returned a non bool type";
				Wg_RaiseException(boolResult->context, WG_EXC_TYPEERROR, message.c_str());
				return nullptr;
			}
			return boolResult;
		}
		case WG_BOP_NOTIN:
			if (Wg_Obj* in = Wg_BinaryOp(WG_BOP_IN, lhs, rhs)) {
				return Wg_UnaryOp(WG_UOP_NOT, in);
			} else {
				return nullptr;
			}
		case WG_BOP_AND: {
			Wg_Obj* lhsb = Wg_UnaryOp(WG_UOP_BOOL, lhs);
			if (lhsb == nullptr)
				return nullptr;
			if (!Wg_GetBool(lhsb))
				return lhsb;
			return Wg_UnaryOp(WG_UOP_BOOL, rhs);
		}
		case WG_BOP_OR: {
			Wg_Obj* lhsb = Wg_UnaryOp(WG_UOP_BOOL, lhs);
			if (lhsb == nullptr)
				return nullptr;
			if (Wg_GetBool(lhsb))
				return lhsb;
			return Wg_UnaryOp(WG_UOP_BOOL, rhs);
		}
		default:
			WG_UNREACHABLE();
		}
	}

	const char* Wg_GetErrorMessage(Wg_Context* context) {
		WG_ASSERT(context);

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
				ss << "Module " << frame.tag;
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
		WG_ASSERT(context);
		return context->currentException;
	}

	void Wg_ClearCurrentException(Wg_Context* context) {
		WG_ASSERT_VOID(context);
		context->currentException = nullptr;
		context->exceptionTrace.clear();
		context->traceMessage.clear();
	}

	void Wg_RaiseException(Wg_Context* context, Wg_Exc type, const char* message) {
		WG_ASSERT_VOID(context);
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
			WG_ASSERT_VOID(false);
		}
	}

	void Wg_RaiseExceptionClass(Wg_Obj* type, const char* message) {
		WG_ASSERT_VOID(type);
		wings::Wg_ObjRef ref(type);

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
		WG_ASSERT_VOID(exception);
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
		WG_ASSERT_VOID(context && given >= 0 && expected >= -1);
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
		WG_ASSERT_VOID(context && argIndex >= 0 && expected);
		std::string msg = "Argument " + std::to_string(argIndex + 1)
			+ " Expected type " + expected;
		Wg_RaiseException(context, WG_EXC_TYPEERROR, msg.c_str());
	}

	void Wg_RaiseAttributeError(const Wg_Obj* obj, const char* attribute) {
		WG_ASSERT_VOID(obj && attribute);
		std::string msg = "'" + wings::WObjTypeToString(obj) +
			"' object has no attribute '" + attribute + "'";
		Wg_RaiseException(obj->context, WG_EXC_ATTRIBUTEERROR, msg.c_str());
	}

	void Wg_RaiseZeroDivisionError(Wg_Context* context) {
		WG_ASSERT_VOID(context);
		Wg_RaiseException(context, WG_EXC_ZERODIVISIONERROR, "division by zero");
	}

	void Wg_RaiseIndexError(Wg_Context* context) {
		WG_ASSERT_VOID(context);
		Wg_RaiseException(context, WG_EXC_INDEXERROR, "index out of range");
	}

	void Wg_RaiseKeyError(Wg_Context* context, Wg_Obj* key) {
		WG_ASSERT_VOID(context);

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
		WG_ASSERT_VOID(context && name);
		std::string msg = "The name '";
		msg += name;
		msg += "' is not defined";
		Wg_RaiseException(context, WG_EXC_NAMEERROR, msg.c_str());
	}

	void Wg_CollectGarbage(Wg_Context* context) {
		WG_ASSERT_VOID(context);

		std::deque<const Wg_Obj*> inUse;
		if (context->currentException)
			inUse.push_back(context->currentException);
		for (const auto& [obj, _] : context->protectedObjects)
			inUse.push_back(obj);
		for (auto& [_, globals] : context->globals)
			for (auto& var : globals)
				inUse.push_back(*var.second);
		for (Wg_Obj* obj : context->kwargs)
			if (obj)
				inUse.push_back(obj);
		for (auto& obj : context->builtins.GetAll())
			if (obj)
				inUse.push_back(obj);

		// Recursively find objects in use
		std::unordered_set<const Wg_Obj*> traversed;
		while (inUse.size()) {
			auto obj = inUse.back();
			inUse.pop_back();
			if (!traversed.contains(obj)) {
				traversed.insert(obj);

				if (Wg_IsTuple(obj) || Wg_IsList(obj)) {
					inUse.insert(
						inUse.end(),
						obj->Get<std::vector<Wg_Obj*>>().begin(),
						obj->Get<std::vector<Wg_Obj*>>().end()
					);
				} else if (Wg_IsDictionary(obj)) {
					for (const auto& [key, value] : obj->Get<wings::WDict>()) {
						inUse.push_back(key);
						inUse.push_back(value);
					}
				} else if (Wg_IsSet(obj)) {
					for (Wg_Obj* value : obj->Get<wings::WSet>()) {
						inUse.push_back(value);
					}
				} else if (Wg_IsFunction(obj)) {
					if (obj->Get<Wg_Obj::Func>().self) {
						inUse.push_back(obj->Get<Wg_Obj::Func>().self);
					}
				} else if (Wg_IsClass(obj)) {
					inUse.insert(
						inUse.end(),
						obj->Get<Wg_Obj::Class>().bases.begin(),
						obj->Get<Wg_Obj::Class>().bases.end()
					);
					obj->Get<Wg_Obj::Class>().instanceAttributes.ForEach([&](auto& entry) {
						inUse.push_back(entry);
						});
				}

				obj->attributes.ForEach([&](auto& entry) {
					inUse.push_back(entry);
					});

				for (Wg_Obj* child : obj->references) {
					inUse.push_back(child);
				}
			}
		}

		// Call finalizers
		for (auto& obj : context->mem)
			if (obj->finalizer.fptr && !traversed.contains(obj.get()))
				obj->finalizer.fptr(obj.get(), obj->finalizer.userdata);

		// Remove unused objects
		context->mem.erase(
			std::remove_if(
				context->mem.begin(),
				context->mem.end(),
				[&traversed](const auto& obj) { return !traversed.contains(obj.get()); }
			),
			context->mem.end()
		);

		context->lastObjectCountAfterGC = context->mem.size();
	}

	void Wg_ProtectObject(const Wg_Obj* obj) {
		WG_ASSERT_VOID(obj);
		size_t& refCount = obj->context->protectedObjects[obj];
		refCount++;
	}

	void Wg_UnprotectObject(const Wg_Obj* obj) {
		WG_ASSERT_VOID(obj);
		auto it = obj->context->protectedObjects.find(obj);
		WG_ASSERT_VOID(it != obj->context->protectedObjects.end());
		if (it->second == 1) {
			obj->context->protectedObjects.erase(it);
		} else {
			it->second--;
		}
	}

	void Wg_LinkReference(Wg_Obj* parent, Wg_Obj* child) {
		WG_ASSERT_VOID(parent && child);
		parent->references.push_back(child);
	}

	void Wg_UnlinkReference(Wg_Obj* parent, Wg_Obj* child) {
		WG_ASSERT_VOID(parent && child);
		auto it = std::find(
			parent->references.begin(),
			parent->references.end(),
			child
		);
		WG_ASSERT_VOID(it != parent->references.end());
		parent->references.erase(it);
	}

} // extern "C"
