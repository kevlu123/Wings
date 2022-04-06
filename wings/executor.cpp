#include "executor.h"
#include "impl.h"

namespace wings {

	void  Executor::PushStack(WObj* obj) {
		WGCProtect(context, obj);
	}

	WObj* Executor::PopStack() {
		WASSERT(!stack.empty());
		auto obj = stack.back();
		stack.pop_back();
		WGCUnprotect(context, obj);
		return obj;
	}

	WObj* Executor::Run(WObj** args, int argc, void* userdata) {
		return ((Executor*)userdata)->Run(args, argc);
	}

	WObj* Executor::Run(WObj** args, int argc) {

		for (size_t pc = 0; pc < instructions.size(); pc++) {
			const auto& instr = instructions[pc];
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
				// TODO
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
