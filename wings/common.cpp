#include "common.h"
#include "lex.h"
#include "parse.h"
#include "executor.h"

#include <algorithm>
#include <unordered_set>

namespace wings {

	Wg_ErrorCallback errorCallback;
	void* errorCallbackUserdata;
	std::mutex errorCallbackMutex;
	
	Wg_Obj* Alloc(Wg_Context* context) {
		// Check allocation limits
		if (context->mem.size() >= context->config.maxAlloc) {
			// Too many objects. Try to free up objects
			Wg_CollectGarbage(context);
			if (context->mem.size() >= context->config.maxAlloc) {
				// If there are still too many objects then set a MemoryException
				Wg_RaiseExceptionObject(context->builtins.memoryErrorInstance);
				return nullptr;
			}
		}

		// Check if GC should run
		size_t threshold = (size_t)((double)context->config.gcRunFactor * context->lastObjectCountAfterGC);
		if (context->mem.size() >= threshold) {
			Wg_CollectGarbage(context);
		}

		// Allocate new object
		auto obj = std::make_unique<Wg_Obj>();
		obj->context = context;

		auto p = obj.get();
		context->mem.push_back(std::move(obj));
		return p;
	}

	void DestroyAllObjects(Wg_Context* context) {
		// Call finalizers
		for (auto& obj : context->mem)
			if (obj->finalizer.fptr)
				obj->finalizer.fptr(obj.get(), obj->finalizer.userdata);

		// Deallocate
		context->mem.clear();
	}

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

	static const std::unordered_set<std::string_view> RESERVED = {
		"True", "False", "None",
		"and", "or", "not",
		"if", "else", "elif", "while", "for",
		"class", "def",
		"try", "except", "finally", "raise", "with", "assert",
		"return", "break", "continue", "pass",
		"global", "nonlocal", "del",
		"from", "import",
		"lambda", "in", "as", "is",
		"await", "async", "yield",
	};

	bool IsKeyword(std::string_view s) {
		return RESERVED.contains(s);
	}

	bool IsValidIdentifier(std::string_view s) {
		if (s.empty())
			return false;

		auto isalpha = [](char c) {
			return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
		};

		auto isalnum = [isalpha](char c) {
			return isalpha(c) || (c >= '0' && c <= '9');
		};
		
		return isalpha(s[0])
			&& std::all_of(s.begin() + 1, s.end(), isalnum)
			&& !IsKeyword(s);
	}
	
	void RegisterMethod(Wg_Obj* klass, const char* name, Wg_Function fptr) {
		Wg_Obj* method = Wg_NewFunction(klass->context, fptr, nullptr, name);
		if (method == nullptr)
			throw LibraryInitException();

		method->Get<Wg_Obj::Func>().isMethod = true;
		if (Wg_IsClass(klass)) {
			Wg_AddAttributeToClass(klass, name, method);
		} else {
			Wg_SetAttribute(klass, name, method);
		}
	}

	Wg_Obj* RegisterFunction(Wg_Context* context, const char* name, Wg_Function fptr) {
		Wg_Obj* obj = Wg_NewFunction(context, fptr, nullptr, name);
		if (obj == nullptr)
			throw LibraryInitException();
		Wg_SetGlobal(context, name, obj);
		return obj;
	}

	Wg_Obj* Compile(Wg_Context* context, const char* code, const char* module, const char* prettyName, bool expr) {
		WG_ASSERT(context && code);

		if (prettyName == nullptr)
			prettyName = wings::DEFAULT_FUNC_NAME;

		auto lexResult = wings::Lex(code);
		auto originalSource = wings::MakeRcPtr<std::vector<std::string>>(lexResult.originalSource);

		auto raiseException = [&](const wings::CodeError& error) {
			std::string_view lineText;
			if (error.srcPos.line < originalSource->size()) {
				lineText = (*originalSource)[error.srcPos.line];
			}
			context->currentTrace.push_back(wings::TraceFrame{
				error.srcPos,
				lineText,
				module,
				prettyName,
				true
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

		Wg_Obj* obj = Wg_NewFunction(context, &wings::DefObject::Run, def);
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

	Wg_Obj* Execute(Wg_Context* context, const char* code, const char* module) {
		if (Wg_Obj* fn = Compile(context, code, module, module, false)) {
			return Wg_Call(fn, nullptr, 0);
		} else {
			return nullptr;
		}
	}
}
