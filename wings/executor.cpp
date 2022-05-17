#include "executor.h"
#include "impl.h"

namespace wings {

	WObj* DefObject::Run(WObj** args, int argc, void* userdata) {
		DefObject* def = (DefObject*)userdata;

		Executor executor{};
		executor.def = def;
		executor.context = def->context;

		// Create local variables
		for (const auto& localVar : def->localVariables) {
			WObj* null = WObjCreateNull(def->context);
			if (null == nullptr)
				return nullptr;
			executor.variables.insert({ localVar, MakeRcPtr<WObj*>(null) });
		}

		// Initialize parameters
		if (argc > (int)def->parameterNames.size()) {
			std::string msg = "function takes " +
				std::to_string(def->parameterNames.size()) +
				" argument(s) but " +
				std::to_string(argc) +
				(argc == 1 ? " was given" : " were given");
			WErrorSetRuntimeError(msg.c_str());
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
			WGcUnprotect(context, val);
	}

	void  Executor::PushStack(WObj* obj) {
		WGcProtect(context, obj);
		stack.push_back(obj);
	}

	WObj* Executor::PopStack() {
		WASSERT(!stack.empty());
		auto obj = stack.back();
		stack.pop_back();
		WGcUnprotect(context, obj);
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
			*it->second = value;
		} else {
			WContextSetGlobal(context, name.c_str(), value);
		}
	}

	WObj* Executor::Run(WObj** args, int argc) {
		for (pc = 0; pc < def->instructions->size(); pc++) {
			DoInstruction((*def->instructions)[pc]);
			if (returnValue.has_value())
				return returnValue.value();
		}
		return WObjCreateNull(context);
	}

	void Executor::DoInstruction(const Instruction& instr) {
		switch (instr.type) {
		case Instruction::Type::Jump: {
			pc = instr.data.jump.location - 1;
			break;
		}
		case Instruction::Type::JumpIfFalse: {
			auto arg = PopStack();
			if (WObjTruthy(arg)) {
				pc = instr.data.jump.location - 1;
			}
			break;
		}
		case Instruction::Type::Pop: {
			PopStack();
			break;
		}
		case Instruction::Type::Return: {
			returnValue = PopStack();
			break;
		}
		case Instruction::Type::Def: {

			DefObject* def = new DefObject();
			def->context = context;
			def->instructions = instr.data.def->instructions;

			for (const auto& param : instr.data.def->parameters)
				def->parameterNames.push_back(param.name);
			for (size_t i = 0; i < instr.data.def->defaultParameterCount; i++) {
				WObj* value = PopStack();
				WGcProtect(context, value);
				def->defaultParameterValues.push_back(value);
			}

			for (const auto& capture : instr.data.def->localCaptures) {
				def->captures.insert({ capture, variables[capture] });
			}
			for (const auto& capture : instr.data.def->globalCaptures) {
				def->captures.insert({ capture, context->globals.at(capture) });
			}
			def->localVariables = instr.data.def->variables;

			WFunc func{};
			func.fptr = &DefObject::Run;
			func.userdata = def;
			WObj* obj = WObjCreateFunc(context, &func);
			if (obj == nullptr) {
				delete def;
				returnValue = nullptr;
				return;
			}

			WFinalizer finalizer{};
			finalizer.fptr = [](WObj* obj, void* userdata) { delete (DefObject*)userdata; };
			finalizer.userdata = def;
			WObjSetFinalizer(obj, &finalizer);

			PushStack(obj);
			break;
		}
		case Instruction::Type::Assign: {
			DoAssignment(*instr.data.operation);
			break;
		}
		case Instruction::Type::Operation: {
			DoOperation(*instr.data.operation);
			break;
		}
		default:
			WUNREACHABLE();
		}
	}

	void Executor::DoLiteral(const Token& t) {
		switch (t.type) {
		case Token::Type::Null:
			PushStack(WObjCreateNull(context));
			break;
		case Token::Type::Bool:
			PushStack(WObjCreateBool(context, t.literal.b));
			break;
		case Token::Type::Int:
			PushStack(WObjCreateInt(context, t.literal.i));
			break;
		case Token::Type::Float:
			PushStack(WObjCreateFloat(context, t.literal.f));
			break;
		case Token::Type::String:
			PushStack(WObjCreateString(context, t.literal.s.c_str()));
			break;
		default:
			WUNREACHABLE();
		}
	}

	void Executor::DoAssignment(const OperationInstructionInfo& op) {
		switch (op.op) {
		case Operation::Assign:
			if (!op.token.text.empty()) {
				// Direct assignment
				SetVariable(op.token.text, PeekStack());
			} else {
				// Assignment to index
			}
			break;
		default:
			WUNREACHABLE();
		}
	}

	void Executor::DoOperation(const OperationInstructionInfo& op) {
		switch (op.op) {
		case Operation::Literal:
			DoLiteral(op.token);
			break;
		case Operation::Variable: {
			WObj* var = GetVariable(op.token.text);
			if (var == nullptr) {
				WErrorSetRuntimeError(("name '" + op.token.text + "' is not defined").c_str());
				returnValue = nullptr;
				return;
			}
			PushStack(var);
			break;
		}
		case Operation::ListLiteral: {
			WObj* li = WObjCreateList(context);
			for (int i = 0; i < op.argc; i++)
				WObjListInsert(li, 0, PopStack());
			PushStack(li);
			break;
		}
		case Operation::MapLiteral: {
			WObj* li = WObjCreateMap(context);
			for (int i = 0; i < op.argc; i++) {
				WObj* val = PopStack();
				WObj* key = PopStack();
				WObjMapSet(li, key, val);
			}
			PushStack(li);
			break;
		}
		case Operation::Call: {
			WObj* fn = stack[stack.size() - op.argc - 1];
			WObj** args = stack.data() + stack.size() - op.argc;
			if (WObj* ret = WObjCall(fn, args, (int)op.argc)) {
				for (size_t i = 0; i < op.argc + 1; i++)
					PopStack();
				PushStack(ret);
			} else {
				returnValue = nullptr;
			}
			break;
		}
		case Operation::Dot: {
			WObj* self = PopStack();
			WObj* attr = WObjGetAttribute(self, op.token.text.c_str());
			if (WObjIsFunc(attr))
				attr->self = self;
			PushStack(attr);
			break;
		}
		default:
			WUNREACHABLE();
		}
	}
}
