#include "executor.h"
#include "common.h"

namespace wings {

	Wg_Obj* DefObject::Run(Wg_Context* context, Wg_Obj** args, int argc) {
		DefObject* def = (DefObject*)Wg_GetFunctionUserdata(context);
		Wg_Obj* kwargs = Wg_GetKwargs(context);
		if (kwargs == nullptr)
			return nullptr;

		Executor executor{};
		executor.def = def;
		executor.context = context;

		// Create local variables
		for (const auto& localVar : def->localVariables) {
			Wg_Obj* null = Wg_None(def->context);
			executor.variables.insert({ localVar, MakeRcPtr<Wg_Obj*>(null) });
		}

		// Add captures
		for (const auto& capture : def->captures) {
			executor.variables.insert(capture);
		}

		// Initialise parameters

		// Set kwargs
		Wg_Obj* newKwargs = nullptr;
		Wg_ObjRef ref;
		if (def->kwArgs.has_value()) {
			newKwargs = Wg_NewDictionary(context);
			if (newKwargs == nullptr)
				return nullptr;
			ref = Wg_ObjRef(newKwargs);
			executor.variables.insert({ def->kwArgs.value(), MakeRcPtr<Wg_Obj*>(newKwargs)});
		}

		std::vector<bool> assignedParams(def->parameterNames.size());
		if (kwargs) {
			for (const auto& [k, value] : kwargs->Get<WDict>()) {
				const char* key = Wg_GetString(k);
				bool found = false;
				for (size_t i = 0; i < def->parameterNames.size(); i++) {
					if (def->parameterNames[i] == key) {
						executor.variables.insert({ key, MakeRcPtr<Wg_Obj*>(value) });
						assignedParams[i] = true;
						found = true;
						break;
					}
				}

				if (!found) {
					if (newKwargs == nullptr) {
						std::string msg;
						if (!def->prettyName.empty())
							msg = def->prettyName + "() ";
						msg += std::string("got an unexpected keyword argument '") + key + "'";
						Wg_RaiseException(context, WG_EXC_TYPEERROR, msg.c_str());
						return nullptr;
					}

					try {
						newKwargs->Get<WDict>()[k] = value;
					} catch (HashException&) {
						return nullptr;
					}
				}
			}
		}

		// Set positional args
		Wg_Obj* listArgs = nullptr;
		if (def->listArgs.has_value()) {
			listArgs = Wg_NewTuple(context, nullptr, 0);
			if (listArgs == nullptr)
				return nullptr;
			executor.variables.insert({ def->listArgs.value(), MakeRcPtr<Wg_Obj*>(listArgs) });
		}

		for (size_t i = 0; i < (size_t)argc; i++) {
			if (i < def->parameterNames.size()) {
				if (assignedParams[i]) {
					std::string msg;
					if (!def->prettyName.empty())
						msg = def->prettyName + "() ";
					msg += "got multiple values for argument '" + def->parameterNames[i] + "'";
					Wg_RaiseException(context, WG_EXC_TYPEERROR, msg.c_str());
					return nullptr;
				}
				executor.variables.insert({ def->parameterNames[i], MakeRcPtr<Wg_Obj*>(args[i]) });
				assignedParams[i] = true;
			} else {
				if (listArgs == nullptr) {
					std::string msg;
					if (!def->prettyName.empty())
						msg = def->prettyName + "() ";
					msg += "takes " + std::to_string(def->parameterNames.size())
						+ " positional argument(s) but " + std::to_string(argc)
						+ (argc == 1 ? " was given" : " were given");
					Wg_RaiseException(context, WG_EXC_TYPEERROR, msg.c_str());
					return nullptr;
				}
				listArgs->Get<std::vector<Wg_Obj*>>().push_back(args[i]);
			}
		}
		
		// Set default args
		size_t defaultableArgsStart = def->parameterNames.size() - def->defaultParameterValues.size();
		for (size_t i = 0; i < def->defaultParameterValues.size(); i++) {
			size_t index = defaultableArgsStart + i;
			if (!assignedParams[index]) {
				executor.variables.insert({ def->parameterNames[index], MakeRcPtr<Wg_Obj*>(def->defaultParameterValues[i]) });
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
			Wg_RaiseException(context, WG_EXC_TYPEERROR, msg.c_str());
			return nullptr;
		}

		context->executors.push_back(&executor);
		auto result = executor.Run();
		context->executors.pop_back();
		return result;
	}

	void  Executor::PushStack(Wg_Obj* obj) {
		stack.push_back(obj);
	}

	Wg_Obj* Executor::PopStack() {
		auto obj = stack.back();
		stack.pop_back();
		return obj;
	}

	void Executor::PopStackUntil(size_t size) {
		while (stack.size() > size)
			PopStack();
	}

	Wg_Obj* Executor::PeekStack() {
		return stack.back();
	}

	void Executor::ClearStack() {
		while (!stack.empty())
			PopStack();
		argFrames = {};
		kwargsStack = {};
	}

	size_t Executor::PopArgFrame() {
		kwargsStack.pop_back();
		size_t ret = stack.size() - argFrames.top();
		argFrames.pop();
		return ret;
	}

	Wg_Obj* Executor::GetVariable(const std::string& name) {
		auto it = variables.find(name);
		if (it != variables.end()) {
			return *it->second;
		} else {
			return Wg_GetGlobal(context, name.c_str());
		}
	}

	void Executor::SetVariable(const std::string& name, Wg_Obj* value) {
		auto it = variables.find(name);
		if (it != variables.end()) {
			*it->second = value;
		} else {
			Wg_SetGlobal(context, name.c_str(), value);
		}
	}

	Wg_Obj* Executor::DirectAssign(const AssignTarget& target, Wg_Obj* value) {
		switch (target.type) {
		case AssignType::Direct:
			SetVariable(target.direct, value);
			return value;
		case AssignType::Pack: {
			std::vector<Wg_ObjRef> values;
			auto f = [](Wg_Obj* value, void* userdata) {
				((std::vector<Wg_ObjRef>*)userdata)->emplace_back(value);
				return true;
			};

			if (!Wg_Iterate(value, &values, f))
				return nullptr;

			if (values.size() != target.pack.size()) {
				Wg_RaiseException(context, WG_EXC_TYPEERROR, "Packed assignment argument count mismatch");
				return nullptr;
			}

			for (size_t i = 0; i < values.size(); i++)
				if (!DirectAssign(target.pack[i], values[i].Get()))
					return nullptr;

			std::vector<Wg_Obj*> buf;
			for (const auto& v : values)
				buf.push_back(v.Get());
			return Wg_NewTuple(context, buf.data(), (int)buf.size());
		}
		default:
			WG_UNREACHABLE();
		}
	}

	Wg_Obj* Executor::Run() {
		auto& frame = context->currentTrace.back();
		frame.module = def->module;
		frame.func = def->prettyName;

		for (pc = 0; pc < def->instructions->size(); pc++) {
			const auto& instr = (*def->instructions)[pc];
			
			auto& frame = context->currentTrace.back();
			frame.lineText = (*def->originalSource)[instr.srcPos.line];
			frame.srcPos = instr.srcPos;

			DoInstruction(instr);

			if (!exitValue.has_value())
				continue;

			// Return normally
			if (exitValue.value() != nullptr)
				break;

			// New exception thrown
			if (tryFrames.empty()) {
				// No handlers. Propagate to next function.
				break;
			}
			
			PopStackUntil(tryFrames.top().stackSize);
			if (tryFrames.top().isHandlingException) {
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

		if (exitValue.has_value()) {
			return exitValue.value();
		} else {
			return Wg_None(context);
		}
	}

	void Executor::DoInstruction(const Instruction& instr) {
		switch (instr.type) {
		case Instruction::Type::Jump:
			pc = instr.jump->location - 1;
			break;
		case Instruction::Type::JumpIfFalsePop:
			if (Wg_Obj* truthy = Wg_UnaryOp(WG_UOP_BOOL, PopStack())) {
				if (!Wg_GetBool(truthy)) {
					pc = instr.jump->location - 1;
				}
			} else {
				exitValue = nullptr;
			}
			break;
		case Instruction::Type::JumpIfFalse:
		case Instruction::Type::JumpIfTrue:
			if (Wg_Obj* truthy = Wg_UnaryOp(WG_UOP_BOOL, PeekStack())) {
				if (Wg_GetBool(truthy) == (instr.type == Instruction::Type::JumpIfTrue)) {
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
			for (size_t i = 0; i < instr.def->defaultParameterCount; i++)
				def->defaultParameterValues.push_back(PopStack());
			def->listArgs = instr.def->listArgs;
			def->kwArgs = instr.def->kwArgs;

			const auto& module = std::string(context->currentModule.top());
			auto& globals = context->globals.at(module);
			
			for (const auto& capture : instr.def->localCaptures) {
				if (variables.contains(capture)) {
					def->captures.insert({ capture, variables[capture] });
				} else {
					if (!globals.contains(capture))
						Wg_SetGlobal(context, capture.c_str(), Wg_None(context));
					
					def->captures.insert({ capture, globals.at(capture) });
				}
			}
			for (const auto& capture : instr.def->globalCaptures) {
				def->captures.insert({ capture, globals.at(capture) });
			}
			def->localVariables = instr.def->variables;

			Wg_Obj* obj = Wg_NewFunction(context, &DefObject::Run, def, instr.def->prettyName.c_str());
			if (obj == nullptr) {
				delete def;
				exitValue = nullptr;
				return;
			}
			obj->Get<Wg_Obj::Func>().isMethod = instr.def->isMethod;

			Wg_RegisterFinalizer(obj, [](void* userdata) { delete (DefObject*)userdata; }, def);

			PushStack(obj);
			break;
		}
		case Instruction::Type::Class: {
			size_t methodCount = instr.klass->methodNames.size();
			size_t baseCount = PopArgFrame();
			auto stackEnd = stack.data() + stack.size();

			std::vector<const char*> methodNames;
			for (const auto& methodName : instr.klass->methodNames)
				methodNames.push_back(methodName.c_str());

			Wg_Obj** bases = stackEnd - baseCount;
			Wg_Obj** methods = stackEnd - methodCount - baseCount;

			Wg_Obj* klass = Wg_NewClass(context, instr.klass->prettyName.c_str(), bases, (int)baseCount);
			if (klass == nullptr) {
				exitValue = nullptr;
				return;
			}

			for (size_t i = 0; i < methodCount; i++)
				AddAttributeToClass(klass, instr.klass->methodNames[i].c_str(), methods[i]);

			for (size_t i = 0; i < methodCount + baseCount; i++)
				PopStack();

			if (klass == nullptr) {
				exitValue = nullptr;
			} else {
				PushStack(klass);
			}
			break;
		}
		case Instruction::Type::Literal: {
			Wg_Obj* value{};
			if (std::holds_alternative<std::nullptr_t>(*instr.literal)) {
				value = Wg_None(context);
			} else if (auto* b = std::get_if<bool>(instr.literal.get())) {
				value = Wg_NewBool(context, *b);
			} else if (auto* i = std::get_if<Wg_int>(instr.literal.get())) {
				value = Wg_NewInt(context, *i);
			} else if (auto* f = std::get_if<Wg_float>(instr.literal.get())) {
				value = Wg_NewFloat(context, *f);
			} else if (auto* s = std::get_if<std::string>(instr.literal.get())) {
				value = Wg_NewStringBuffer(context, s->c_str(), (int)s->size());
			} else {
				WG_UNREACHABLE();
			}

			if (value) {
				PushStack(value);
			} else {
				exitValue = nullptr;
			}
			break;
		}
		case Instruction::Type::Tuple:
		case Instruction::Type::List:
		case Instruction::Type::Set: {
			Wg_Obj* (*creator)(Wg_Context*, Wg_Obj**, int) = nullptr;
			switch (instr.type) {
			case Instruction::Type::Tuple: creator = Wg_NewTuple; break;
			case Instruction::Type::List: creator = Wg_NewList; break;
			case Instruction::Type::Set: creator = Wg_NewSet; break;
			}
			size_t argc = PopArgFrame();
			Wg_Obj** argv = stack.data() + stack.size() - argc;
			if (Wg_Obj* li = creator(context, argv, (int)argc)) {
				for (size_t i = 0; i < argc; i++)
					PopStack();
				PushStack(li);
			} else {
				exitValue = nullptr;
			}
			break;
		}
		case Instruction::Type::Map:
			if (Wg_Obj* dict = Wg_NewDictionary(context)) {
				size_t argc = PopArgFrame();
				Wg_Obj** start = stack.data() + stack.size() - argc;
				for (size_t i = 0; i < argc / 2; i++) {
					Wg_Obj* key = start[2 * i];
					Wg_Obj* val = start[2 * i + 1];
					Wg_ObjRef ref(dict);
					try {
						dict->Get<WDict>()[key] = val;
					} catch (HashException&) {
						exitValue = nullptr;
						return;
					}
				}

				for (size_t i = 0; i < argc; i++)
					PopStack();
				PushStack(dict);
			} else {
				exitValue = nullptr;
			}
			break;
		case Instruction::Type::Variable:
			if (Wg_Obj* value = GetVariable(instr.string->string)) {
				PushStack(value);
			} else {
				Wg_RaiseNameError(context, instr.string->string.c_str());
				exitValue = nullptr;
			}
			break;
		case Instruction::Type::DirectAssign: {
			if (Wg_Obj* v = DirectAssign(instr.directAssign->assignTarget, PopStack())) {
				PushStack(v);
			} else {
				exitValue = nullptr;
			}
			break;
		}
		case Instruction::Type::MemberAssign: {
			Wg_Obj* value = PopStack();
			Wg_Obj* obj = PopStack();
			Wg_SetAttribute(obj, instr.string->string.c_str(), value);
			PushStack(value);
			break;
		}
		case Instruction::Type::PushArgFrame:
			argFrames.push(stack.size());
			kwargsStack.push_back({});
			break;
		case Instruction::Type::Call: {
			size_t kwargc = kwargsStack.back().size();
			size_t argc = stack.size() - argFrames.top() - kwargc - 1;

			Wg_Obj* fn = stack[stack.size() - argc - kwargc - 1];
			Wg_Obj** args = stack.data() + stack.size() - argc - kwargc;
			Wg_Obj** kwargsv = stack.data() + stack.size() - kwargc;

			Wg_Obj* kwargs = nullptr;
			if (kwargc) {
				kwargs = Wg_NewDictionary(context, kwargsStack.back().data(), kwargsv, (int)kwargc);
				if (kwargs == nullptr) {
					exitValue = nullptr;
					return;
				}
			}

			if (Wg_Obj* ret = Wg_Call(fn, args, (int)argc, kwargs)) {
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
			Wg_Obj* obj = PopStack();
			if (Wg_Obj* attr = Wg_GetAttribute(obj, instr.string->string.c_str())) {
				PushStack(attr);
			} else {
				exitValue = nullptr;
			}
			break;
		}
		case Instruction::Type::Unpack: {
			Wg_Obj* iterable = PopStack();

			auto f = [](Wg_Obj* value, void* userdata) {
				Executor* executor = (Executor*)userdata;
				executor->PushStack(value);
				return true;
			};

			if (!Wg_Iterate(iterable, this, f)) {
				exitValue = nullptr;
			}
			break;
		}
		case Instruction::Type::UnpackMapForMapCreation: {
			Wg_Obj* map = PopStack();
			if (!Wg_IsDictionary(map)) {
				Wg_RaiseException(context, WG_EXC_TYPEERROR, "Unary '**' must be applied to a dictionary");
				exitValue = nullptr;
				return;
			}

			for (const auto& [key, value] : map->Get<WDict>()) {
				PushStack(key);
				PushStack(value);
			}
			break;
		}
		case Instruction::Type::UnpackMapForCall: {
			Wg_Obj* map = PopStack();
			if (!Wg_IsDictionary(map)) {
				Wg_RaiseException(context, WG_EXC_TYPEERROR, "Unary '**' must be applied to a dictionary");
				exitValue = nullptr;
				return;
			}

			for (const auto& [key, value] : map->Get<WDict>()) {
				if (!Wg_IsString(key)) {
					Wg_RaiseException(context, WG_EXC_TYPEERROR, "Keywords must be strings");
					exitValue = nullptr;
					return;
				}
				kwargsStack.back().push_back(key);
				PushStack(value);
			}
			break;
		}
		case Instruction::Type::PushKwarg:
			kwargsStack.back().push_back(PopStack());
			break;
		case Instruction::Type::Not: {
			Wg_Obj* arg = Wg_UnaryOp(WG_UOP_BOOL, PopStack());
			if (arg == nullptr) {
				exitValue = nullptr;
				break;
			}

			if (Wg_Obj* value = Wg_NewBool(context, !Wg_GetBool(arg))) {
				PushStack(value);
			} else {
				exitValue = nullptr;
			}
			break;
		}
		case Instruction::Type::Is:
			PushStack(Wg_NewBool(context, PopStack() == PopStack()));
			break;
		case Instruction::Type::Raise: {
			Wg_Obj* expr = PopStack();
			if (Wg_IsClass(expr)) {
				Wg_RaiseExceptionClass(expr);
			} else {
				Wg_RaiseExceptionObject(expr);
			}
			exitValue = nullptr;
			break;
		}
		case Instruction::Type::PushTry:
			tryFrames.push({
				instr.pushTry->exceptJump,
				instr.pushTry->finallyJump,
				false,
				stack.size()
				});
			break;
		case Instruction::Type::PopTry:
			tryFrames.pop();
			if (Wg_GetException(context))
				exitValue = nullptr;
			break;
		case Instruction::Type::Except:
			Wg_ClearException(context);
			break;
		case Instruction::Type::CurrentException:
			PushStack(Wg_GetException(context));
			break;
		case Instruction::Type::IsInstance:
			PushStack(context->builtins.isinstance);
			break;
		case Instruction::Type::Slice: {
			Wg_Obj* slice = Wg_Call(context->builtins.slice, &context->builtins.none, 1);
			if (slice == nullptr) {
				exitValue = nullptr;
				break;
			}

			Wg_Obj* step = PopStack();
			Wg_Obj* stop = PopStack();
			Wg_Obj* start = PopStack();
			Wg_SetAttribute(slice, "step", step);
			Wg_SetAttribute(slice, "stop", stop);
			Wg_SetAttribute(slice, "start", start);
			PushStack(slice);
			break;
		}
		case Instruction::Type::Import: {
			const char* alias = instr.import->alias.empty() ? nullptr : instr.import->alias.c_str();
			if (Wg_ImportModule(context, instr.import->module.c_str(), alias) == nullptr)
				exitValue = nullptr;
			break;
		}
		case Instruction::Type::ImportFrom: {
			const char* moduleName = instr.importFrom->module.c_str();
			if (instr.importFrom->names.empty()) {
				if (!Wg_ImportAllFromModule(context, moduleName))
					exitValue = nullptr;
			} else if (!instr.importFrom->alias.empty()) {
				if (!Wg_ImportFromModule(context, moduleName, instr.importFrom->names[0].c_str(), instr.importFrom->alias.c_str()))
					exitValue = nullptr;
			} else {
				for (const auto& name : instr.importFrom->names) {
					if (!Wg_ImportFromModule(context, moduleName, name.c_str())) {
						exitValue = nullptr;
						break;
					}
				}
			}
			break;
		}
		default:
			WG_UNREACHABLE();
		}
	}

	void Executor::GetReferences(std::deque<const Wg_Obj*>& refs) {
		for (const auto& var : variables)
			refs.push_back(*var.second);
		for (const auto& frame : kwargsStack)
			for (const auto& kwarg : frame)
				refs.push_back(kwarg);
		for (const auto& val : this->stack)
			refs.push_back(val);
	}
}
