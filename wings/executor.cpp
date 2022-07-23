#include "executor.h"
#include "impl.h"

namespace wings {

	WObj* DefObject::Run(WObj** args, int argc, WObj* kwargs, void* userdata) {
		DefObject* def = (DefObject*)userdata;
		WContext* context = def->context;

		Executor executor{};
		executor.def = def;
		executor.context = context;

		// Create local variables
		for (const auto& localVar : def->localVariables) {
			WObj* null = WCreateNoneType(def->context);
			executor.variables.insert({ localVar, MakeRcPtr<WObj*>(null) });
		}

		// Add captures
		for (const auto& capture : def->captures) {
			executor.variables.insert(capture);
		}

		// Initialize parameters

		// Set kwargs
		WObj* newKwargs = nullptr;
		if (def->kwArgs.has_value()) {
			newKwargs = WCreateDictionary(context);
			if (newKwargs == nullptr)
				return nullptr;
			executor.variables.insert({ def->kwArgs.value(), MakeRcPtr<WObj*>(newKwargs)});
		}

		std::vector<bool> assignedParams(def->parameterNames.size());
		for (const auto& [k, value] : kwargs->m) {
			const char* key = WGetString(k);
			bool found = false;
			for (size_t i = 0; i < def->parameterNames.size(); i++) {
				if (def->parameterNames[i] == key) {
					executor.variables.insert({ key, MakeRcPtr<WObj*>(value) });
					assignedParams[i] = true;
					found = true;
					break;
				}
			}

			if (!found) {
				if (newKwargs == nullptr) {
					//WRaiseError(context, );
					return nullptr;
				}
				newKwargs->m.insert({ k, value });
			}
		}

		// Set positional args
		WObj* listArgs = nullptr;
		if (def->listArgs.has_value()) {
			listArgs = WCreateTuple(context, nullptr, 0);
			if (listArgs == nullptr)
				return nullptr;
			executor.variables.insert({ def->listArgs.value(), MakeRcPtr<WObj*>(listArgs) });
		}

		for (int i = 0; i < argc; i++) {
			if (i < def->parameterNames.size()) {
				if (assignedParams[i]) {
					//WRaiseError(context, );
					return nullptr;
				}
				executor.variables.insert({ def->parameterNames[i], MakeRcPtr<WObj*>(args[i]) });
				assignedParams[i] = true;
			} else {
				if (listArgs == nullptr) {
					//WRaiseError(context, );
					return nullptr;
				}
				listArgs->v.push_back(args[i]);
			}
		}
		
		// Set default args
		size_t defaultableArgsStart = def->parameterNames.size() - def->defaultParameterValues.size();
		for (size_t i = 0; i < def->defaultParameterValues.size(); i++) {
			size_t index = defaultableArgsStart + i;
			if (!assignedParams[index]) {
				executor.variables.insert({ def->parameterNames[index], MakeRcPtr<WObj*>(def->defaultParameterValues[i]) });
				assignedParams[index] = true;
			}
		}

		// Check for unassigned arguments
		std::string unassigned;
		for (size_t i = 0; i < def->parameterNames.size(); i++)
			if (!assignedParams[i])
				unassigned += std::to_string(i + 1) + ", ";
		if (!unassigned.empty()) {
			unassigned.pop_back();
			unassigned.pop_back();
			std::string msg = "Function " +
				def->prettyName + "()"
				+ " missing parameter(s) "
				+ unassigned;
			WRaiseError(context, msg.c_str());
			return nullptr;
		}

		return executor.Run(args, argc, kwargs);
	}

	DefObject::~DefObject() {
		for (WObj* val : defaultParameterValues)
			WUnprotectObject(val);
	}

	void  Executor::PushStack(WObj* obj) {
		WProtectObject(obj);
		stack.push_back(obj);
	}

	WObj* Executor::PopStack() {
		WASSERT(!stack.empty());
		auto obj = stack.back();
		stack.pop_back();
		WUnprotectObject(obj);
		return obj;
	}

	WObj* Executor::PeekStack() {
		WASSERT(!stack.empty());
		return stack.back();
	}

	void Executor::ClearStack() {
		while (!stack.empty())
			PopStack();
		argFrames = {};
		kwargsStack = {};
	}

	size_t Executor::PopArgFrame() {
		kwargsStack.pop();
		size_t ret = stack.size() - argFrames.top();
		argFrames.pop();
		return ret;
	}

	WObj* Executor::GetVariable(const std::string& name) {
		auto it = variables.find(name);
		if (it != variables.end()) {
			return *it->second;
		} else {
			return WGetGlobal(context, name.c_str());
		}
	}

	void Executor::SetVariable(const std::string& name, WObj* value) {
		auto it = variables.find(name);
		if (it != variables.end()) {
			if (*it->second != value) {
				WUnprotectObject(*it->second);
				WProtectObject(value);
				*it->second = value;
			}
		} else {
			WSetGlobal(context, name.c_str(), value);
		}
	}

	WObj* Executor::DirectAssign(const AssignTarget& target, WObj* value) {
		switch (target.type) {
		case AssignType::Direct:
			SetVariable(target.direct, value);
			return value;
		case AssignType::Pack: {
			std::vector<WObj*> values;
			auto f = [](WObj* value, void* userdata) {
				WProtectObject(value);
				((std::vector<WObj*>*)userdata)->push_back(value);
				return true;
			};

			auto unprotectValues = [&] {
				for (WObj* v : values)
					WUnprotectObject(v);
			};

			if (!WIterate(value, &values, f)) {
				unprotectValues();
				return nullptr;
			}

			if (values.size() != target.pack.size()) {
				WRaiseError(context, "Packed assignment argument count mismatch.");
				unprotectValues();
				return nullptr;
			}

			for (size_t i = 0; i < values.size(); i++)
				if (!DirectAssign(target.pack[i], values[i])) {
					unprotectValues();
					return nullptr;
				}

			unprotectValues();
			return WCreateTuple(context, values.data(), (int)values.size());
		}
		default:
			WUNREACHABLE();
		}
	}

	WObj* Executor::Run(WObj** args, int argc, WObj* kwargs) {
		for (const auto& var : variables)
			WProtectObject(*var.second);

		for (pc = 0; pc < def->instructions->size(); pc++) {
			const auto& instr = (*def->instructions)[pc];
			DoInstruction(instr);

			auto pushTrace = [&] {
				context->err.trace.push_back({
					instr.srcPos,
					(*def->originalSource)[instr.srcPos.line],
					def->module,
					def->prettyName
					});
			};

			if (!exitValue.has_value())
				continue;

			// Return normally
			if (exitValue.value() != nullptr)
				break;

			// Some non-exception error
			if (WGetCurrentException(context) == nullptr) {
				pushTrace();
				break;
			}

			// New exception thrown
			ClearStack();
			if (tryFrames.empty()) {
				// No handlers. Propagate to next function.
				pushTrace();
				break;
			} else if (tryFrames.top().isHandlingException) {
				// Was already handling an exception. Jump to finally.
				pc = tryFrames.top().finallyJump - 1;
				exitValue.reset();
			} else {
				// Jump to handler
				pc = tryFrames.top().exceptJump - 1;
				tryFrames.top().isHandlingException = true;
				exitValue.reset();
			}
		}

		ClearStack();
		for (const auto& var : variables)
			WUnprotectObject(*var.second);

		if (exitValue.has_value()) {
			return exitValue.value();
		} else {
			return WCreateNoneType(context);
		}
	}

	void Executor::DoInstruction(const Instruction& instr) {
		switch (instr.type) {
		case Instruction::Type::Jump:
			pc = instr.jump->location - 1;
			break;
		case Instruction::Type::JumpIfFalse:
			if (WObj* truthy = WTruthy(PopStack())) {
				if (!WGetBool(truthy)) {
					pc = instr.jump->location - 1;
				}
			} else {
				exitValue = nullptr;
			}
			break;
		case Instruction::Type::Pop:
			PopStack();
			break;
		case Instruction::Type::Return:
			exitValue = PopStack();
			break;
		case Instruction::Type::Def: {
			DefObject* def = new DefObject();
			def->context = context;
			def->module = this->def->module;
			def->prettyName = instr.def->prettyName;
			def->instructions = instr.def->instructions;
			def->originalSource = this->def->originalSource;

			for (const auto& param : instr.def->parameters)
				def->parameterNames.push_back(param.name);
			for (size_t i = 0; i < instr.def->defaultParameterCount; i++) {
				WObj* value = PopStack();
				WProtectObject(value);
				def->defaultParameterValues.push_back(value);
			}
			def->listArgs = instr.def->listArgs;
			def->kwArgs = instr.def->kwArgs;

			for (const auto& capture : instr.def->localCaptures) {
				if (variables.contains(capture)) {
					def->captures.insert({ capture, variables[capture] });
				} else {
					if (!context->globals.contains(capture))
						WSetGlobal(context, capture.c_str(), WCreateNoneType(context));
					def->captures.insert({ capture, context->globals.at(capture) });
				}
			}
			for (const auto& capture : instr.def->globalCaptures) {
				def->captures.insert({ capture, context->globals.at(capture) });
			}
			def->localVariables = instr.def->variables;

			WFunc func{};
			func.fptr = &DefObject::Run;
			func.userdata = def;
			func.isMethod = instr.def->isMethod;
			WObj* obj = WCreateFunction(context, &func);
			if (obj == nullptr) {
				delete def;
				exitValue = nullptr;
				return;
			}

			WFinalizer finalizer{};
			finalizer.fptr = [](WObj* obj, void* userdata) { delete (DefObject*)userdata; };
			finalizer.userdata = def;
			WSetFinalizer(obj, &finalizer);

			PushStack(obj);
			break;
		}
		case Instruction::Type::Class: {
			size_t methodCount = instr._class->methodNames.size();
			size_t baseCount = PopArgFrame();
			auto stackEnd = stack.data() + stack.size();

			std::vector<const char*> methodNames;
			for (const auto& methodName : instr._class->methodNames)
				methodNames.push_back(methodName.c_str());

			WClass wclass{};
			wclass.methodCount = (int)methodCount;
			wclass.methods = stackEnd - methodCount - baseCount;
			wclass.methodNames = methodNames.data();
			wclass.bases = stackEnd - baseCount;
			wclass.baseCount = (int)baseCount;
			WObj* _class = WCreateClass(context, &wclass);

			for (size_t i = 0; i < methodCount + baseCount; i++)
				PopStack();

			if (_class == nullptr) {
				exitValue = nullptr;
			} else {
				PushStack(_class);
			}
			break;
		}
		case Instruction::Type::Literal: {
			WObj* value{};
			if (auto* n = std::get_if<std::nullptr_t>(instr.literal.get())) {
				value = WCreateNoneType(context);
			} else if (auto* b = std::get_if<bool>(instr.literal.get())) {
				value = WCreateBool(context, *b);
			} else if (auto* i = std::get_if<wint>(instr.literal.get())) {
				value = WCreateInt(context, *i);
			} else if (auto* f = std::get_if<wfloat>(instr.literal.get())) {
				value = WCreateFloat(context, *f);
			} else if (auto* s = std::get_if<std::string>(instr.literal.get())) {
				value = WCreateString(context, s->c_str());
			} else {
				WUNREACHABLE();
			}

			if (value) {
				PushStack(value);
			} else {
				exitValue = nullptr;
			}
			break;
		}
		case Instruction::Type::Tuple:
		case Instruction::Type::List: {
			auto creator = instr.type == Instruction::Type::Tuple ? WCreateTuple : WCreateList;
			size_t argc = PopArgFrame();
			WObj** argv = stack.data() + stack.size() - argc;
			if (WObj* li = creator(context, argv, (int)argc)) {
				for (size_t i = 0; i < argc; i++)
					PopStack();
				PushStack(li);
			} else {
				exitValue = nullptr;
			}
			break;
		}
		case Instruction::Type::Map:
			if (WObj* li = WCreateDictionary(context)) {
				size_t argc = PopArgFrame();
				WObj** start = stack.data() + stack.size() - argc;
				for (size_t i = 0; i < argc / 2; i++) {
					WObj* key = start[2 * i];
					WObj* val = start[2 * i + 1];
					if (!WIsImmutableType(key)) {
						WRaiseError(context, "Only an immutable type can be used as a dictionary key");
						exitValue = nullptr;
						return;
					}
					li->m[key] = val;
				}

				for (size_t i = 0; i < argc; i++)
					PopStack();
				PushStack(li);
			} else {
				exitValue = nullptr;
			}
			break;
		case Instruction::Type::Variable:
			if (WObj* value = GetVariable(instr.string->string)) {
				PushStack(value);
			} else {
				std::string msg = "The name '" + instr.string->string + "' is not defined";
				WRaiseError(context, msg.c_str());
				exitValue = nullptr;
			}
			break;
		case Instruction::Type::DirectAssign: {
			if (WObj* v = DirectAssign(instr.directAssign->assignTarget, PopStack())) {
				PushStack(v);
			} else {
				exitValue = nullptr;
			}
			break;
		}
		case Instruction::Type::MemberAssign: {
			WObj* value = PopStack();
			WObj* obj = PopStack();
			WSetAttribute(obj, instr.string->string.c_str(), value);
			PushStack(value);
			break;
		}
		case Instruction::Type::PushArgFrame:
			argFrames.push(stack.size());
			kwargsStack.push({});
			break;
		case Instruction::Type::Call: {
			size_t kwargc = kwargsStack.top().size();
			size_t argc = stack.size() - argFrames.top() - kwargc - 1;

			WObj* fn = stack[stack.size() - argc - kwargc - 1];
			WObj** args = stack.data() + stack.size() - argc - kwargc;
			WObj** kwargsv = stack.data() + stack.size() - kwargc;

			WObj* kwargs = WCreateDictionary(context, kwargsStack.top().data(), kwargsv, (int)kwargc);
			if (kwargs == nullptr) {
				exitValue = nullptr;
				return;
			}

			if (WObj* ret = WCall(fn, args, (int)argc, kwargs)) {
				for (size_t i = 0; i < argc + kwargc + 1; i++)
					PopStack();
				PushStack(ret);
			} else {
				exitValue = nullptr;
			}
			PopArgFrame();
			break;
		}
		case Instruction::Type::Dot: {
			WObj* obj = PopStack();
			if (WObj* attr = WGetAttribute(obj, instr.string->string.c_str())) {
				PushStack(attr);
			} else {
				std::string msg = "Object of type " + WObjTypeToString(obj->type) +
					" has no attribute " + instr.string->string;
				WRaiseError(context, msg.c_str());
				exitValue = nullptr;
			}
			break;
		}
		case Instruction::Type::Unpack: {
			WObj* iterable = PopStack();

			auto f = [](WObj* value, void* userdata) {
				Executor* executor = (Executor*)userdata;
				executor->PushStack(value);
				return true;
			};

			if (!WIterate(iterable, this, f)) {
				exitValue = nullptr;
			}
			break;
		}
		case Instruction::Type::UnpackMapForMapCreation: {
			WObj* map = PopStack();
			if (!WIsDictionary(map)) {
				WRaiseError(context, "Unary '**' must be applied to a dictionary");
				exitValue = nullptr;
				return;
			}

			for (const auto& [key, value] : map->m) {
				PushStack(key);
				PushStack(value);
			}
			break;
		}
		case Instruction::Type::UnpackMapForCall: {
			WObj* map = PopStack();
			if (!WIsDictionary(map)) {
				WRaiseError(context, "Unary '**' must be applied to a dictionary");
				exitValue = nullptr;
				return;
			}

			for (const auto& [key, value] : map->m) {
				if (!WIsString(key)) {
					WRaiseError(context, "Keywords arguments must be strings");
					exitValue = nullptr;
					return;
				}
				kwargsStack.top().push_back(key);
				PushStack(value);
			}
			break;
		}
		case Instruction::Type::PushKwarg:
			kwargsStack.top().push_back(PopStack());
			break;
		case Instruction::Type::And: {
			WObj* arg1 = WTruthy(PopStack());
			if (arg1 == nullptr) {
				exitValue = nullptr;
				break;
			}

			if (!WGetBool(arg1)) {
				// Short circuit
				if (WObj* value = WCreateBool(context, false)) {
					PushStack(value);
				} else {
					exitValue = nullptr;
					break;
				}
			}

			WObj* arg2 = WTruthy(PopStack());
			if (arg2 == nullptr) {
				exitValue = nullptr;
				break;
			}
			
			if (WObj* value = WCreateBool(context, WGetBool(arg2))) {
				PushStack(value);
			} else {
				exitValue = nullptr;
			}
			break;
		}
		case Instruction::Type::Or: {
			WObj* arg1 = WTruthy(PopStack());
			if (arg1 == nullptr) {
				exitValue = nullptr;
				break;
			}

			if (WGetBool(arg1)) {
				// Short circuit
				if (WObj* value = WCreateBool(context, true)) {
					PushStack(value);
				} else {
					exitValue = nullptr;
					break;
				}
			}

			WObj* arg2 = WTruthy(PopStack());
			if (arg2 == nullptr) {
				exitValue = nullptr;
				break;
			}

			if (WObj* value = WCreateBool(context, WGetBool(arg2))) {
				PushStack(value);
			} else {
				exitValue = nullptr;
			}
			break;
		}
		case Instruction::Type::Not: {
			WObj* arg = WTruthy(PopStack());
			if (arg == nullptr) {
				exitValue = nullptr;
				break;
			}

			if (WObj* value = WCreateBool(context, !WGetBool(arg))) {
				PushStack(value);
			} else {
				exitValue = nullptr;
			}
			break;
		}
		case Instruction::Type::In: {
			WObj* obj = PopStack();
			WObj* container = PopStack();
			if (WObj* value = WIn(container, obj)) {
				PushStack(value);
			} else {
				exitValue = nullptr;
			}
			break;
		}
		case Instruction::Type::NotIn: {
			WObj* obj = PopStack();
			WObj* container = PopStack();
			if (WObj* value = WNotIn(container, obj)) {
				PushStack(value);
			} else {
				exitValue = nullptr;
			}
			break;
		}
		case Instruction::Type::ListComprehension: {
			WObj* expr = stack[stack.size() - 3];
			WObj* assign = stack[stack.size() - 2];
			WObj* iterable = stack[stack.size() - 1];

			WObj* list = WCreateList(context);
			if (list == nullptr) {
				exitValue = nullptr;
				return;
			}
			WProtectObject(list);

			struct State {
				WObj* expr;
				WObj* assign;
				WObj* list;
			} state = { expr, assign, list };

			bool success = WIterate(iterable, &state, [](WObj* value, void* userdata) {
				State& state = *(State*)userdata;

				if (WCall(state.assign, &value, 1) == nullptr)
					return false;

				WObj* entry = WCall(state.expr, nullptr, 0);
				if (entry == nullptr)
					return false;

				return (bool)WCallMethod(state.list, "append", &entry, 1);
				});

			WUnprotectObject(list);
			for (int i = 0; i < 3; i++)
				PopStack();

			if (!success) {
				exitValue = nullptr;
			} else {
				PushStack(list);
			}
			break;
		}
		case Instruction::Type::Raise: {
			WObj* expr = PopStack();
			if (WIsClass(expr)) {
				expr = WCall(expr, nullptr, 0);
				if (expr == nullptr) {
					exitValue = nullptr;
					break;
				}
			}
			context->currentException = expr;
			exitValue = nullptr;
			break;
		}
		case Instruction::Type::PushTry:
			tryFrames.push({ instr.pushTry->exceptJump, instr.pushTry->finallyJump });
			break;
		case Instruction::Type::PopTry:
			tryFrames.pop();
			if (WGetCurrentException(context))
				exitValue = nullptr;
			break;
		case Instruction::Type::Except:
			WClearCurrentException(context);
			break;
		case Instruction::Type::CurrentException:
			PushStack(WGetCurrentException(context));
			break;
		case Instruction::Type::IsInstance:
			PushStack(context->isinstance);
			break;
		case Instruction::Type::SliceClass:
			PushStack(context->builtinClasses.slice);
			break;
		default:
			WUNREACHABLE();
		}
	}
}
