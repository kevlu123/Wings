#include "dismodule.h"
#include "common.h"
#include "compile.h"
#include "executor.h"

#include <queue>

namespace wings {	
	static std::string AssignTargetToString(const AssignTarget& target) {
		if (target.type == AssignType::Direct) {
			return target.direct;
		} else {
			std::string s = "(";
			for (const auto& child : target.pack) {
				s += AssignTargetToString(child);
				s += ", ";
			}
			s.pop_back();
			s.pop_back();
			return s;
		}
	}

	static std::string LiteralToString(const LiteralInstruction& literal) {
		if (std::holds_alternative<std::nullptr_t>(literal)) {
			return "None";
		} else if (std::holds_alternative<bool>(literal)) {
			return std::get<bool>(literal) ? "True" : "False";
		} else if (std::holds_alternative<Wg_int>(literal)) {
			return std::to_string(std::get<Wg_int>(literal));
		} else if (std::holds_alternative<Wg_float>(literal)) {
			return std::to_string(std::get<Wg_float>(literal));
		} else if (std::holds_alternative<std::string>(literal)) {
			return "\"" + std::get<std::string>(literal) + "\"";
		} else {
			WG_UNREACHABLE();
		}
	}

	static std::string PadLeft(size_t i, size_t size) {
		auto n = std::to_string(i);
		while (n.size() < size)
			n.insert(0, 1, ' ');
		return n;
	}

	static Wg_Obj* dis(Wg_Context* context, Wg_Obj** argv, int argc) {
		WG_EXPECT_ARG_COUNT(1);
		WG_EXPECT_ARG_TYPE_FUNC(0);

		const auto& fn = argv[0]->Get<Wg_Obj::Func>();
		if (fn.fptr != &DefObject::Run) {
			Wg_RaiseException(context, WG_EXC_TYPEERROR, "Cannot disassemble native function");
			return nullptr;
		}

		struct Func {
			const std::vector<Instruction>* instructions;
			std::string_view name;
		};

		std::queue<Func> functions;
		DefObject* def = (DefObject*)fn.userdata;
		functions.push(Func{ &*def->instructions, def->prettyName });

		std::string s;
		while (!functions.empty()) {
			s += "Function ";
			s += functions.front().name;
			s += "()\n";
			const auto* instructions = functions.front().instructions;
			functions.pop();
			
			for (size_t i = 0; i < instructions->size(); i++) {
				const Instruction& instr = (*instructions)[i];

				if (i == 0 || instr.srcPos.line != (*instructions)[i - 1].srcPos.line) {
					if (i)
						s += "\n";
					s += PadLeft(instr.srcPos.line + 1, 6) + " ";
				} else {
					s += "       ";
				}
				s += PadLeft(i, 4) + " ";

				switch (instr.type) {
				case Instruction::Type::DirectAssign:
					if (instr.directAssign->assignTarget.type == AssignType::Direct) {
						s += "ASSIGN\t\t";
					} else {
						s += "ASSIGN_PACK\t\t";
					}
					s += AssignTargetToString(instr.directAssign->assignTarget);
					break;
				case Instruction::Type::MemberAssign:
					s += "ASSIGN_ATTR\t\t" + instr.string->string;
					break;
				case Instruction::Type::Literal:
					s += "LOAD_CONST\t\t" + LiteralToString(*instr.literal);
					break;
				case Instruction::Type::Call:
					s += "CALL";
					break;
				case Instruction::Type::Return:
					s += "RETURN";
					break;
				case Instruction::Type::Pop:
					s += "POP";
					break;
				case Instruction::Type::PushArgFrame:
					s += "BEGIN_ARGS";
					break;
				case Instruction::Type::Dot:
					s += "GET_ATTR\t\t" + instr.string->string;
					break;
				case Instruction::Type::Variable:
					s += "LOAD_VAR\t\t" + instr.string->string;
					break;
				case Instruction::Type::Jump:
					s += "JUMP\t\tto " + std::to_string(instr.jump->location);
					break;
				case Instruction::Type::JumpIfFalse:
					s += "JUMP_IF_FALSE\tto " + std::to_string(instr.jump->location);
					break;
				case Instruction::Type::List:
					s += "MAKE_LIST";
					break;
				case Instruction::Type::Tuple:
					s += "MAKE_TUPLE";
					break;
				case Instruction::Type::Map:
					s += "MAKE_DICT";
					break;
				case Instruction::Type::Set:
					s += "MAKE_SET";
					break;
				case Instruction::Type::Slice:
					s += "MAKE_SLICE";
					break;
				case Instruction::Type::Raise:
					s += "RAISE";
					break;
				case Instruction::Type::PushTry:
					s += "BEGIN_TRY\t\t" + std::to_string(instr.pushTry->exceptJump)
						+ ", " + std::to_string(instr.pushTry->finallyJump);
					break;
				case Instruction::Type::PopTry:
					s += "END_TRY";
					break;
				case Instruction::Type::CurrentException:
					s += "LOAD_CUR_EXCEPT";
					break;
				case Instruction::Type::IsInstance:
					s += "LOAD_IS_INSTANCE";
					break;
				case Instruction::Type::Except:
					s += "HANDLE_EXCEPT";
					break;
				case Instruction::Type::Import:
					s += "IMPORT\t\t" + instr.import->module;
					if (!instr.import->alias.empty())
						s += " as " + instr.import->alias;
					break;
				case Instruction::Type::ImportFrom:
					if (instr.importFrom->names.empty()) {
						s += "IMPORT_ALL\t\t" + instr.importFrom->module;
					} else if (!instr.importFrom->alias.empty()) {
						s += "IMPORT_FROM\t\tfrom " + instr.importFrom->module
							+ " import " + instr.importFrom->names[0]
							+ " as " + instr.importFrom->alias;
					} else {
						s += "IMPORT_FROM\t\tfrom " + instr.importFrom->module + " import ";
						for (const auto& name : instr.importFrom->names) {
							s += name + ", ";
						}
						s.pop_back();
						s.pop_back();
					}
					break;
				case Instruction::Type::Is:
					s += "IS";
					break;
				case Instruction::Type::PushKwarg:
					s += "PUSH_KWARG";
					break;
				case Instruction::Type::UnpackMapForCall:
					s += "UNPACK_KWARGS";
					break;
				case Instruction::Type::UnpackMapForMapCreation:
					s += "UNPACK_DICT";
					break;
				case Instruction::Type::Unpack:
					s += "UNPACK_ITERABLE";
					break;
				case Instruction::Type::Class:
					s += "MAKE_CLASS\t\t" + instr.klass->prettyName + " [";
					for (const auto& name : instr.klass->methodNames) {
						s += name + ", ";
					}
					s.pop_back();
					s.pop_back();
					s += "]";
					break;
				case Instruction::Type::Def:
					s += "MAKE_FUNCTION\t" + instr.def->prettyName;

					functions.push(Func{ &*instr.def->instructions, instr.def->prettyName });
					break;
				default:
					s += "???";
					break;
				}

				s += "\n";
			}

			s += "\n";
		}

		Wg_Print(context, s.c_str(), (int)s.size());

		return Wg_None(context);
	}

	bool ImportDis(Wg_Context* context) {
		try {
			RegisterFunction(context, "dis", dis);
			return true;
		} catch (LibraryInitException&) {
			return false;
		}
	}
}
