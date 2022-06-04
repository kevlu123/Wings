#include "compile.h"
#include "impl.h"
#include <unordered_map>

namespace wings {

	static thread_local std::vector<size_t> breakInstructions;
	static thread_local std::vector<size_t> continueInstructions;

	static void CompileBody(const Statement& node, std::vector<Instruction>& instructions);

	static const std::unordered_map<Operation, OpInstruction> OP_DATA = {
		{ Operation::Index,  { "__getitem__", 2 } },
		{ Operation::Pos,	 { "__pos__", 1 } },
		{ Operation::Neg,	 { "__neg__", 1 } },
		{ Operation::Add,	 { "__add__", 2 } },
		{ Operation::Sub,	 { "__sub__", 2 } },
		{ Operation::Mul,	 { "__mul__", 2 } },
		{ Operation::Div,	 { "__div__", 2 } },
		{ Operation::IDiv,	 { "__floordiv__", 2 } },
		{ Operation::Mod,	 { "__mod__", 2 } },
		{ Operation::Pow,	 { "__pow__", 2 } },
		{ Operation::Eq,	 { "__eq__", 2 } },
		{ Operation::Ne,	 { "__ne__", 2 } },
		{ Operation::Lt,	 { "__lt__", 2 } },
		{ Operation::Le,	 { "__ge__", 2 } },
		{ Operation::Gt,	 { "__gt__", 2 } },
		{ Operation::Ge,	 { "__ge__", 2 } },
		{ Operation::In,	 { "__contains__", 2 } },
		{ Operation::BitAnd, { "__and__", 2 } },
		{ Operation::BitOr,  { "__or__", 2 } },
		{ Operation::BitNot, { "__invert__", 1 } },
		{ Operation::BitXor, { "__xor__", 2 } },
		{ Operation::ShiftL, { "__lshift__", 2 } },
		{ Operation::ShiftR, { "__rshift__", 2 } },
	};

	static void CompileExpression(const Expression& expression, std::vector<Instruction>& instructions) {
		if (expression.assignType == AssignType::None) {
			for (size_t i = 0; i < expression.children.size(); i++) {
				CompileExpression(expression.children[i], instructions);
			}
		}

		Instruction instr{};
		instr.srcPos = expression.srcPos;

		switch (expression.assignType) {
		case AssignType::Direct:
			// <assign>
			//		<assignee>
			//		<expr>
			CompileExpression(expression.children[1], instructions);
			instr.directAssign = std::make_unique<DirectAssignInstruction>();
			instr.directAssign->variableName = expression.children[0].variableName;
			instr.type = Instruction::Type::DirectAssign;
			break;
		case AssignType::Index:
			// <assign>
			//		<assignee>
			//			<var>
			//			<index>
			//		<expr>
			CompileExpression(expression.children[0].children[0], instructions);
			CompileExpression(expression.children[0].children[1], instructions);
			CompileExpression(expression.children[1], instructions);
			instr.op = std::make_unique<OpInstruction>();
			instr.op->argc = 2;
			instr.op->operation = "__setitem__";
			instr.type = Instruction::Type::Operation;
			break;
		case AssignType::Member:
			// <assign>
			//		<assignee>
			//			<var>
			//		<expr>
			CompileExpression(expression.children[0], instructions);
			CompileExpression(expression.children[1], instructions);
			instr.memberAccess = std::make_unique<MemberAccessInstruction>();
			instr.memberAccess->memberName = expression.children[0].variableName;
			instr.type = Instruction::Type::MemberAssign;
			break;
		case AssignType::None:
			switch (expression.operation) {
			case Operation::Literal:
				instr.literal = std::make_unique<LiteralInstruction>();
				switch (expression.literalValue.type) {
				case LiteralValue::Type::Null: *instr.literal = nullptr; break;
				case LiteralValue::Type::Bool: *instr.literal = expression.literalValue.b; break;
				case LiteralValue::Type::Int: *instr.literal = expression.literalValue.i; break;
				case LiteralValue::Type::Float: *instr.literal = expression.literalValue.f; break;
				case LiteralValue::Type::String: *instr.literal = expression.literalValue.s; break;
				default: WUNREACHABLE();
				}
				instr.type = Instruction::Type::Literal;
				break;
			case Operation::ListLiteral:
				instr.variadicOp = std::make_unique<VariadicOpInstruction>();
				instr.variadicOp->argc = expression.children.size();
				instr.type = Instruction::Type::ListLiteral;
				break;
			case Operation::MapLiteral:
				instr.variadicOp = std::make_unique<VariadicOpInstruction>();
				instr.variadicOp->argc = expression.children.size() / 2;
				instr.type = Instruction::Type::MapLiteral;
				break;
			case Operation::Variable:
				instr.variable = std::make_unique<VariableLoadInstruction>();
				instr.variable->variableName = expression.variableName;
				instr.type = Instruction::Type::Variable;
				break;
			case Operation::Dot:
				instr.memberAccess = std::make_unique<MemberAccessInstruction>();
				instr.memberAccess->memberName = expression.variableName;
				instr.type = Instruction::Type::Dot;
				break;
			case Operation::Call:
				instr.variadicOp = std::make_unique<VariadicOpInstruction>();
				instr.variadicOp->argc = expression.children.size();
				instr.type = Instruction::Type::Call;
				break;
			case Operation::And:
				CompileExpression(expression.children[0], instructions);
				CompileExpression(expression.children[1], instructions);
				instr.type = Instruction::Type::And;
				break;
			case Operation::Or:
				CompileExpression(expression.children[0], instructions);
				CompileExpression(expression.children[1], instructions);
				instr.type = Instruction::Type::Or;
				break;
			case Operation::Not:
				CompileExpression(expression.children[0], instructions);
				instr.type = Instruction::Type::Not;
				break;
			case Operation::In:
				CompileExpression(expression.children[0], instructions);
				CompileExpression(expression.children[1], instructions);
				instr.type = Instruction::Type::NotIn;
				break;
			case Operation::NotIn:
				CompileExpression(expression.children[0], instructions);
				CompileExpression(expression.children[1], instructions);
				instr.type = Instruction::Type::NotIn;
				break;
			default:
				instr.op = std::make_unique<OpInstruction>();
				*instr.op = OP_DATA.at(expression.operation);
				instr.type = Instruction::Type::Operation;
				break;
			}
			break;
		default:
			WUNREACHABLE();
		}

		instructions.push_back(std::move(instr));
	}

	static void CompileExpressionStatement(const Statement& node, std::vector<Instruction>& instructions) {
		CompileExpression(node.expr, instructions);

		Instruction instr{};
		instr.srcPos = node.expr.srcPos;
		instr.type = Instruction::Type::Pop;
		instructions.push_back(std::move(instr));
	}

	static void CompileIf(const Statement& node, std::vector<Instruction>& instructions) {
		CompileExpression(node.expr, instructions);

		size_t falseJumpInstrIndex = instructions.size();
		Instruction falseJump{};
		falseJump.srcPos = node.srcPos;
		falseJump.type = Instruction::Type::JumpIfFalse;
		falseJump.jump = std::make_unique<JumpInstruction>();
		instructions.push_back(std::move(falseJump));

		CompileBody(node, instructions);

		if (node.elseClause) {
			size_t trueJumpInstrIndex = instructions.size();
			Instruction trueJump{};
			trueJump.srcPos = node.elseClause->srcPos;
			trueJump.type = Instruction::Type::Jump;
			trueJump.jump = std::make_unique<JumpInstruction>();
			instructions.push_back(std::move(trueJump));

			instructions[falseJumpInstrIndex].jump->location = instructions.size();

			CompileBody(*node.elseClause, instructions);

			instructions[trueJumpInstrIndex].jump->location = instructions.size();
		} else {
			instructions[falseJumpInstrIndex].jump->location = instructions.size();
		}
	}

	static void CompileWhile(const Statement& node, std::vector<Instruction>& instructions) {
		size_t conditionLocation = instructions.size();
		CompileExpression(node.expr, instructions);
		
		size_t terminateJumpInstrIndex = instructions.size();
		Instruction terminateJump{};
		terminateJump.srcPos = node.srcPos;
		terminateJump.type = Instruction::Type::JumpIfFalse;
		terminateJump.jump = std::make_unique<JumpInstruction>();
		instructions.push_back(std::move(terminateJump));

		CompileBody(node, instructions);

		Instruction loopJump{};
		loopJump.srcPos = node.srcPos;
		loopJump.type = Instruction::Type::Jump;
		loopJump.jump = std::make_unique<JumpInstruction>();
		loopJump.jump->location = conditionLocation;
		instructions.push_back(std::move(loopJump));

		instructions[terminateJumpInstrIndex].jump->location = instructions.size();

		if (node.elseClause) {
			CompileBody(*node.elseClause, instructions);
		}

		for (size_t index : breakInstructions) {
			instructions[index].jump->location = instructions.size();
		}
		for (size_t index : continueInstructions) {
			instructions[index].jump->location = conditionLocation;
		}
		breakInstructions.clear();
		continueInstructions.clear();
	}

	static void CompileBreak(const Statement& node, std::vector<Instruction>& instructions) {
		breakInstructions.push_back(instructions.size());

		Instruction jump{};
		jump.srcPos = node.srcPos;
		jump.type = Instruction::Type::Jump;
		jump.jump = std::make_unique<JumpInstruction>();
		instructions.push_back(std::move(jump));
	}

	static void CompileContinue(const Statement& node, std::vector<Instruction>& instructions) {
		continueInstructions.push_back(instructions.size());

		Instruction jump{};
		jump.srcPos = node.srcPos;
		jump.type = Instruction::Type::Jump;
		jump.jump = std::make_unique<JumpInstruction>();
		instructions.push_back(std::move(jump));
	}

	static void CompileReturn(const Statement& node, std::vector<Instruction>& instructions) {
		CompileExpression(node.expr, instructions);

		Instruction in{};
		in.srcPos = node.srcPos;
		in.type = Instruction::Type::Return;
		instructions.push_back(std::move(in));
	}

	static void CompileDef(const Statement& node, std::vector<Instruction>& instructions) {
		const auto& parameters = node.def.parameters;
		size_t defaultParamCount = 0;
		for (size_t i = parameters.size(); i--> 0; ) {
			const auto& param = parameters[i];
			if (param.defaultValue.has_value()) {
				CompileExpression(param.defaultValue.value(), instructions);
				defaultParamCount = parameters.size() - i;
			} else {
				break;
			}
		}

		Instruction def{};
		def.srcPos = node.srcPos;
		def.type = Instruction::Type::Def;
		def.def = std::make_unique<DefInstruction>();
		def.def->variables = std::vector<std::string>(
			node.def.variables.begin(),
			node.def.variables.end()
			);
		def.def->localCaptures = std::vector<std::string>(
			node.def.localCaptures.begin(),
			node.def.localCaptures.end()
			);
		def.def->globalCaptures = std::vector<std::string>(
			node.def.globalCaptures.begin(),
			node.def.globalCaptures.end()
			);
		def.def->defaultParameterCount = defaultParamCount;
		def.def->parameters = node.def.parameters;
		def.def->instructions = MakeRcPtr<std::vector<Instruction>>();
		def.def->prettyName = node.def.name;
		CompileBody(node, *def.def->instructions);
		instructions.push_back(std::move(def));

		Instruction assign{};
		assign.srcPos = node.srcPos;
		assign.type = Instruction::Type::DirectAssign;
		assign.directAssign = std::make_unique<DirectAssignInstruction>();
		assign.directAssign->variableName = node.def.name;
		instructions.push_back(std::move(assign));
	}

	using CompileFn = void(*)(const Statement&, std::vector<Instruction>&);

	static const std::unordered_map<Statement::Type, CompileFn> COMPILE_FUNCTIONS = {
		{ Statement::Type::Expr, CompileExpressionStatement },
		{ Statement::Type::If, CompileIf },
		{ Statement::Type::While, CompileWhile },
		{ Statement::Type::Break, CompileBreak },
		{ Statement::Type::Continue, CompileContinue },
		{ Statement::Type::Return, CompileReturn },
		{ Statement::Type::Def, CompileDef },
	};

	static void CompileStatement(const Statement& node, std::vector<Instruction>& instructions) {
		if (COMPILE_FUNCTIONS.contains(node.type)) {
			COMPILE_FUNCTIONS.at(node.type)(node, instructions);
		}
	}

	static void CompileBody(const Statement& node, std::vector<Instruction>& instructions) {
		for (const auto& child : node.body) {
			CompileStatement(child, instructions);
		}
	}

	std::vector<Instruction> Compile(const Statement& parseTree) {
		breakInstructions.clear();
		continueInstructions.clear();

		std::vector<Instruction> instructions;
		CompileBody(parseTree, instructions);

		return instructions;
	}

}
