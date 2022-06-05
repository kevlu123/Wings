#include "executor.h"
#include "impl.h"

namespace wings {

	WObj* DefObject::Run(WObj** args, int argc, void* userdata) {
		DefObject* def = (DefObject*)userdata;
		WContext* context = def->context;

		Executor executor{};
		executor.def = def;
		executor.context = context;

		// Create local variables
		for (const auto& localVar : def->localVariables) {
			WObj* null = WObjCreateNull(def->context);
			executor.variables.insert({ localVar, MakeRcPtr<WObj*>(null) });
		}

		// Add captures
		for (const auto& capture : def->captures) {
			executor.variables.insert(capture);
		}

		// Initialize parameters
		if ((size_t)argc > def->parameterNames.size() || (size_t)argc < def->parameterNames.size() - def->defaultParameterValues.size()) {
			std::string msg = "function takes " +
				std::to_string(def->parameterNames.size()) +
				" argument(s) but " +
				std::to_string(argc) +
				(argc == 1 ? " was given" : " were given");
			WErrorSetRuntimeError(context, msg.c_str());
			return nullptr;
		}
		for (size_t i = 0; i < def->parameterNames.size(); i++) {
			size_t defaultParameterIndex = i + def->defaultParameterValues.size() - def->parameterNames.size();
			WObj* value = ((int)i < argc) ? args[i] : def->defaultParameterValues[defaultParameterIndex];
			executor.variables.insert({ def->parameterNames[i], MakeRcPtr<WObj*>(value) });
		}

		return executor.Run(args, argc);
	}

	DefObject::~DefObject() {
		for (WObj* val : defaultParameterValues)
			WGcUnprotect(val);
	}

	void  Executor::PushStack(WObj* obj) {
		WGcProtect(obj);
		stack.push_back(obj);
	}

	WObj* Executor::PopStack() {
		WASSERT(!stack.empty());
		auto obj = stack.back();
		stack.pop_back();
		WGcUnprotect(obj);
		return obj;
	}

	WObj* Executor::PeekStack() {
		WASSERT(!stack.empty());
		return stack.back();
	}

	WObj* Executor::GetVariable(const std::string& name) {
		auto it = variables.find(name);
		if (it != variables.end()) {
			return *it->second;
		} else {
			return WContextGetGlobal(context, name.c_str());
		}
	}

	void Executor::SetVariable(const std::string& name, WObj* value) {
		auto it = variables.find(name);
		if (it != variables.end()) {
			if (*it->second != value) {
				WGcUnprotect(*it->second);
				WGcProtect(value);
				*it->second = value;
			}
		} else {
			WContextSetGlobal(context, name.c_str(), value);
		}
	}

	WObj* Executor::Run(WObj** args, int argc) {
		for (const auto& var : variables)
			WGcProtect(*var.second);

		for (pc = 0; pc < def->instructions->size(); pc++) {
			const auto& instr = (*def->instructions)[pc];
			DoInstruction(instr);
			if (exitValue.has_value()) {
				if (exitValue.value() == nullptr) {
					context->err.trace.push_back({
						instr.srcPos,
						(*def->originalSource)[instr.srcPos.line],
						def->module,
						def->prettyName
						});
				}
				goto end;
			}
		}

	end:
		for (const auto& var : variables)
			WGcUnprotect(*var.second);

		if (exitValue.has_value()) {
			return exitValue.value();
		} else {
			return WObjCreateNull(context);
		}
	}

	void Executor::DoInstruction(const Instruction& instr) {
		switch (instr.type) {
		case Instruction::Type::Jump:
			pc = instr.jump->location - 1;
			break;
		case Instruction::Type::JumpIfFalse:
			if (WObj* truthy = WOpTruthy(PopStack())) {
				if (!WObjGetBool(truthy)) {
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
				WGcProtect(value);
				def->defaultParameterValues.push_back(value);
			}

			for (const auto& capture : instr.def->localCaptures) {
				if (variables.contains(capture)) {
					def->captures.insert({ capture, variables[capture] });
				} else {
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
			WObj* obj = WObjCreateFunc(context, &func);
			if (obj == nullptr) {
				delete def;
				exitValue = nullptr;
				return;
			}

			WFinalizer finalizer{};
			finalizer.fptr = [](WObj* obj, void* userdata) { delete (DefObject*)userdata; };
			finalizer.userdata = def;
			WObjSetFinalizer(obj, &finalizer);

			PushStack(obj);
			break;
		}
		case Instruction::Type::Literal: {
			WObj* value{};
			if (auto* n = std::get_if<std::nullptr_t>(instr.literal.get())) {
				value = WObjCreateNull(context);
			} else if (auto* b = std::get_if<bool>(instr.literal.get())) {
				value = WObjCreateBool(context, *b);
			} else if (auto* i = std::get_if<wint>(instr.literal.get())) {
				value = WObjCreateInt(context, *i);
			} else if (auto* f = std::get_if<wfloat>(instr.literal.get())) {
				value = WObjCreateFloat(context, *f);
			} else if (auto* s = std::get_if<std::string>(instr.literal.get())) {
				value = WObjCreateString(context, s->c_str());
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
		case Instruction::Type::ListLiteral:
			if (WObj* li = WObjCreateList(context)) {
				for (int i = 0; i < instr.variadicOp->argc; i++)
					li->v.insert(li->v.begin(), PopStack());
				PushStack(li);
			} else {
				exitValue = nullptr;
			}
			break;
		case Instruction::Type::MapLiteral:
			if (WObj* li = WObjCreateMap(context)) {
				for (int i = 0; i < instr.variadicOp->argc; i++) {
					WObj* val = PopStack();
					WObj* key = PopStack();
					if (!WObjIsImmutableType(key)) {
						WErrorSetRuntimeError(context, "Only an immutable type can be used as a dictionary key");
						exitValue = nullptr;
						return;
					}
					li->m.insert({ *key, val });
				}
				PushStack(li);
			} else {
				exitValue = nullptr;
			}
			break;
		case Instruction::Type::Variable:
			if (WObj* value = GetVariable(instr.variable->variableName)) {
				PushStack(value);
			} else {
				std::string msg = "The name '" + instr.variable->variableName + "' is not defined";
				WErrorSetRuntimeError(context, msg.c_str());
				exitValue = nullptr;
			}
			break;
		case Instruction::Type::DirectAssign:
			SetVariable(instr.directAssign->variableName, PeekStack());
			break;
		case Instruction::Type::MemberAssign: {
			WObj* value = PopStack();
			WObj* obj = PopStack();
			WObjSetAttribute(obj, instr.memberAccess->memberName.c_str(), value);
			break;
		}
		case Instruction::Type::Call: {
			size_t argc = instr.variadicOp->argc;
			WObj* fn = stack[stack.size() - argc];
			WObj** args = stack.data() + stack.size() - argc + 1;
			if (WObj* ret = WOpCall(fn, args, (int)argc - 1)) {
				for (size_t i = 0; i < argc; i++)
					PopStack();
				PushStack(ret);
			} else {
				exitValue = nullptr;
			}
			break;
		}
		case Instruction::Type::Dot: {
			WObj* obj = PopStack();
			if (WObj* attr = WObjGetAttribute(obj, instr.memberAccess->memberName.c_str())) {
				PushStack(attr);
			} else {
				std::string msg = "Object of type " + WObjTypeToString(obj->type) +
					" has no attribute " + instr.memberAccess->memberName;
				WErrorSetRuntimeError(context, msg.c_str());
				exitValue = nullptr;
			}
			break;
		}
		case Instruction::Type::Operation: {
			std::vector<WObj*> args;
			for (size_t i = 0; i < instr.op->argc - 1; i++) {
				args.push_back(PopStack());
			}

			if (WObj* res = WOpCallMethod(PopStack(), instr.op->operation.c_str(), args.data(), (int)args.size())) {
				PushStack(res);
			} else {
				exitValue = nullptr;
			}
			break;
		}
		case Instruction::Type::And: {
			WObj* arg1 = WOpTruthy(PopStack());
			if (arg1 == nullptr) {
				exitValue = nullptr;
				break;
			}

			if (!WObjGetBool(arg1)) {
				// Short circuit
				if (WObj* value = WObjCreateBool(context, false)) {
					PushStack(value);
				} else {
					exitValue = nullptr;
					break;
				}
			}

			WObj* arg2 = WOpTruthy(PopStack());
			if (arg2 == nullptr) {
				exitValue = nullptr;
				break;
			}
			
			if (WObj* value = WObjCreateBool(context, WObjGetBool(arg2))) {
				PushStack(value);
			} else {
				exitValue = nullptr;
			}
			break;
		}
		case Instruction::Type::Or: {
			WObj* arg1 = WOpTruthy(PopStack());
			if (arg1 == nullptr) {
				exitValue = nullptr;
				break;
			}

			if (WObjGetBool(arg1)) {
				// Short circuit
				if (WObj* value = WObjCreateBool(context, true)) {
					PushStack(value);
				} else {
					exitValue = nullptr;
					break;
				}
			}

			WObj* arg2 = WOpTruthy(PopStack());
			if (arg2 == nullptr) {
				exitValue = nullptr;
				break;
			}

			if (WObj* value = WObjCreateBool(context, WObjGetBool(arg2))) {
				PushStack(value);
			} else {
				exitValue = nullptr;
			}
			break;
		}
		case Instruction::Type::Not: {
			WObj* arg = WOpTruthy(PopStack());
			if (arg == nullptr) {
				exitValue = nullptr;
				break;
			}

			if (WObj* value = WObjCreateBool(context, WObjGetBool(arg))) {
				PushStack(value);
			} else {
				exitValue = nullptr;
			}
			break;
		}
		case Instruction::Type::In: {
			WObj* obj = PopStack();
			WObj* container = PopStack();
			if (WObj* value = WOpIn(container, obj)) {
				PushStack(value);
			} else {
				exitValue = nullptr;
			}
			break;
		}
		case Instruction::Type::NotIn: {
			WObj* obj = PopStack();
			WObj* container = PopStack();
			if (WObj* value = WOpNotIn(container, obj)) {
				PushStack(value);
			} else {
				exitValue = nullptr;
			}
			break;
		}
		default:
			WUNREACHABLE();
		}
	}
}
