#include "executor.h"
#include "impl.h"

namespace wings {

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

	bool Executor::InitializeParams(WObj** args, int argc) {
		if (argc > (int)parameterNames.size()) {
			// TODO: error message
			return false;
		} else {
			for (size_t i = 0; i < parameterNames.size(); i++) {
				size_t defaultParameterIndex = i + defaultParameterValues.size() - parameterNames.size();
				WObj* value = ((int)i < argc) ? args[i] : defaultParameterValues[defaultParameterIndex];
				SetVariable(parameterNames[i], value);
			}
			return true;
		}
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

	WObj* Executor::Run(WObj** args, int argc, void* userdata) {
		return ((Executor*)userdata)->Run(args, argc);
	}

	WObj* Executor::Run(WObj** args, int argc) {

		if (!InitializeParams(args, argc)) {
			return nullptr;
		}

		for (pc = 0; pc < instructions->size(); pc++) {
			DoInstruction((*instructions)[pc]);
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

			Executor* executor = new Executor();
			executor->context = context;
			executor->instructions = instr.data.def->instructions;

			for (const auto& capture : instr.data.def->localCaptures) {
				executor->variables.insert({ capture, variables[capture] });
			}
			for (const auto& capture : instr.data.def->globalCaptures) {
				executor->variables.insert({ capture, context->globals.at(capture) });
			}

			for (size_t i = 0; i < instr.data.def->defaultParameterCount; i++) {
				WObj* value = PopStack();
				executor->defaultParameterValues.push_back(value);
			}

			WFunc func{};
			func.fptr = &Executor::Run;
			func.userdata = executor;
			WObj* obj = WObjCreateFunc(context, &func);
			if (obj == nullptr) {
				delete (Executor*)executor;
				returnValue = nullptr;
				return;
			}

			WFinalizer finalizer{};
			finalizer.fptr = [](WObj* obj, void* userdata) { delete (Executor*)userdata; };
			finalizer.userdata = executor;
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
		case Operation::Variable:
			PushStack(GetVariable(op.token.text));
			break;
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
			WObj** args = &stack[stack.size() - op.argc];
			WObj* ret = WObjCall(fn, args, (int)op.argc);
			for (size_t i = 0; i < op.argc + 1; i++)
				PopStack();
			PushStack(ret);
			break;
		}
		default:
			WUNREACHABLE();
		}
	}
}
