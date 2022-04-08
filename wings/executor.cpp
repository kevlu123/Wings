#include "executor.h"
#include "impl.h"

namespace wings {

	void  Executor::PushStack(WObj* obj) {
		WGcProtect(context, obj);
	}

	WObj* Executor::PopStack() {
		WASSERT(!stack.empty());
		auto obj = stack.back();
		stack.pop_back();
		WGcUnprotect(context, obj);
		return obj;
	}

	WObj* Executor::Run(WObj** args, int argc, void* userdata) {
		return ((Executor*)userdata)->Run(args, argc);
	}

	WObj* Executor::Run(WObj** args, int argc) {

		for (size_t pc = 0; pc < instructions->size(); pc++) {
			const auto& instr = (*instructions)[pc];
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
				return PopStack();
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
					return nullptr;
				}

				WFinalizer finalizer{};
				finalizer.fptr = [](WObj* obj, void* userdata) { delete (Executor*)userdata; };
				finalizer.userdata = executor;
				WObjSetFinalizer(obj, &finalizer);

				PushStack(obj);
				break;
			}
			case Instruction::Type::Operation: {
				// TODO
				break;
			}
			default:
				WUNREACHABLE();
			}
		}

		return WObjCreateNull(context);
	}
}
