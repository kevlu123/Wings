#include "common.h"
#include "lex.h"
#include "parse.h"
#include "executor.h"

#include <algorithm>
#include <unordered_set>

namespace wings {

	std::atomic<Wg_ErrorCallback> errorCallback;
	
	Wg_Obj* Alloc(Wg_Context* context) {
		// Objects should never be allocated while the garbage collector is running.
		WG_ASSERT(!context->gcRunning);
		
		// Check allocation limits
		if (context->mem.size() >= (size_t)context->config.maxAlloc) {
			// Too many objects. Try to free up objects
			Wg_CollectGarbage(context);
			if (context->mem.size() >= (size_t)context->config.maxAlloc) {
				// If there are still too many objects then set a MemoryException
				Wg_RaiseException(context, WG_EXC_MEMORYERROR);
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

	void CallErrorCallback(const char* message) {
		Wg_ErrorCallback cb = errorCallback;

		if (cb) {
			cb(message);
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
		return CodeError{ true, {}, {} };
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
		if (Wg_IsClass(klass)) {
			if (Wg_BindMethod(klass, name, fptr, nullptr) == nullptr)
				throw LibraryInitException();
		} else {
			Wg_Obj* method = Wg_NewFunction(klass->context, fptr, nullptr, name);
			if (method == nullptr)
				throw LibraryInitException();
			method->Get<Wg_Obj::Func>().isMethod = true;
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

	void AddAttributeToClass(Wg_Obj* klass, const char* attribute, Wg_Obj* value) {
		WG_ASSERT_VOID(klass && attribute && value && Wg_IsClass(klass) && IsValidIdentifier(attribute));
		klass->Get<Wg_Obj::Class>().instanceAttributes.Set(attribute, value);
	}

	Wg_Obj* Compile(Wg_Context* context, const char* code, const char* module, const char* prettyName, bool expr) {
		WG_ASSERT(context && code);

		if (prettyName == nullptr)
			prettyName = DEFAULT_FUNC_NAME;

		auto lexResult = Lex(code);
		auto originalSource = MakeRcPtr<std::vector<std::string>>(lexResult.originalSource);

		auto raiseException = [&](const CodeError& error) {
			std::string_view lineText;
			if (error.srcPos.line < originalSource->size()) {
				lineText = (*originalSource)[error.srcPos.line];
			}
			context->currentTrace.push_back(TraceFrame{
				error.srcPos,
				lineText,
				module,
				prettyName,
				true
				});

			Wg_RaiseException(context, WG_EXC_SYNTAXERROR, error.message.c_str());

			context->currentTrace.pop_back();
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
			std::vector<Statement> body = std::move(parseResult.parseTree.expr.def.body);
			if (body.size() != 1 || !std::holds_alternative<stat::Expr>(body[0].data)) {
				raiseException(CodeError::Bad("Invalid syntax"));
				return nullptr;
			}

			stat::Return ret;
			ret.expr = std::move(body[0].Get<stat::Expr>().expr);
			Statement stat;
			stat.srcPos = body[0].srcPos;
			stat.data = std::move(ret);

			parseResult.parseTree.expr.def.body.clear();
			parseResult.parseTree.expr.def.body.push_back(std::move(stat));
		}

		auto* def = new DefObject();
		def->context = context;
		def->module = module;
		def->prettyName = prettyName;
		def->originalSource = std::move(originalSource);
		auto instructions = Compile(parseResult.parseTree);
		def->instructions = MakeRcPtr<std::vector<Instruction>>(std::move(instructions));

		Wg_Obj* obj = Wg_NewFunction(context, &DefObject::Run, def);
		if (obj == nullptr) {
			delete def;
			return nullptr;
		}
		
		Wg_RegisterFinalizer(obj, [](void* ud) { delete (DefObject*)ud; }, def);

		return obj;
	}

	Wg_Obj* Execute(Wg_Context* context, const char* code, const char* module) {
		if (Wg_Obj* fn = Compile(context, code, module, module, false)) {
			return Wg_Call(fn, nullptr, 0);
		} else {
			return nullptr;
		}
	}

	Rng::Rng() :
		engine(std::random_device()())
	{
	}

	void Rng::Seed(Wg_int seed) {
		engine.seed((unsigned long long)seed);
		dist.reset();
	}

	Wg_float Rng::Rand() {
		return dist(engine);
	}

	Wg_int Rng::Int(Wg_int minIncl, Wg_int maxIncl) {
		auto i = (Wg_int)((maxIncl - minIncl + 1) * Rand() + minIncl);

		if (i > maxIncl) // Just in case
			return maxIncl;

		return i;
	}

	Wg_float Rng::Float(Wg_float minIncl, Wg_float maxIncl) {
		return (maxIncl - minIncl) * Rand() + minIncl;
	}

	std::mt19937_64& Rng::Engine() {
		return engine;
	}
}
