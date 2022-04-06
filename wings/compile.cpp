#include "compile.h"
#include <unordered_map>

namespace wings {

	Instruction::~Instruction() {
		switch (type) {
		case Instruction::Type::Operation:
			if (data.operation) {
				delete data.operation;
			}
			break;
		case Instruction::Type::Def:
			if (data.def) {
				delete data.def;
			}
			break;
		}
	}

	Instruction::Instruction(Instruction&& rhs) noexcept :
		Instruction()
	{
		*this = std::move(rhs);
	}

	Instruction& Instruction::operator=(Instruction&& rhs) noexcept {
		std::swap(type, rhs.type);
		std::swap(data, rhs.data);
		return *this;
	}

	static thread_local std::vector<size_t> breakInstructions;
	static thread_local std::vector<size_t> continueInstructions;

	static void CompileBody(const Statement& node, std::vector<Instruction>& instructions);

	static void CompileExpression(const Expression& expression, std::vector<Instruction>& instructions) {
		for (const auto& child : expression.children) {
			CompileExpression(child, instructions);
		}

		Instruction instr{};
		instr.type = Instruction::Type::Operation;
		instr.data.operation = new OperationInstructionInfo;
		instr.data.operation->op = expression.operation;
		instr.data.operation->token = expression.literal;
		instructions.push_back(std::move(instr));
	}

	static void CompileExpressionStatement(const Statement& node, std::vector<Instruction>& instructions) {
		CompileExpression(node.expr, instructions);

		Instruction instr{};
		instr.type = Instruction::Type::Pop;
		instructions.push_back(std::move(instr));
	}

	static void CompileIf(const Statement& node, std::vector<Instruction>& instructions) {
		CompileExpression(node.expr, instructions);

		size_t falseJumpInstrIndex = instructions.size();
		Instruction falseJump{};
		falseJump.type = Instruction::Type::JumpIfFalse;
		instructions.push_back(std::move(falseJump));

		CompileBody(node, instructions);

		if (node.elseClause) {
			size_t trueJumpInstrIndex = instructions.size();
			Instruction trueJump{};
			trueJump.type = Instruction::Type::Jump;
			instructions.push_back(std::move(trueJump));

			instructions[falseJumpInstrIndex].data.jump.location = instructions.size();

			CompileBody(*node.elseClause, instructions);

			instructions[trueJumpInstrIndex].data.jump.location = instructions.size();
		} else {
			instructions[falseJumpInstrIndex].data.jump.location = instructions.size();
		}
	}

	static void CompileWhile(const Statement& node, std::vector<Instruction>& instructions) {
		size_t conditionLocation = instructions.size();
		CompileExpression(node.expr, instructions);
		
		size_t terminateJumpInstrIndex = instructions.size();
		Instruction terminateJump{};
		terminateJump.type = Instruction::Type::JumpIfFalse;
		instructions.push_back(std::move(terminateJump));

		CompileBody(node, instructions);

		Instruction loopJump{};
		loopJump.type = Instruction::Type::Jump;
		loopJump.data.jump.location = conditionLocation;
		instructions.push_back(std::move(loopJump));

		instructions[terminateJumpInstrIndex].data.jump.location = instructions.size();

		if (node.elseClause) {
			CompileBody(*node.elseClause, instructions);
		}

		for (size_t index : breakInstructions) {
			instructions[index].data.jump.location = instructions.size();
		}
		for (size_t index : continueInstructions) {
			instructions[index].data.jump.location = conditionLocation;
		}
		breakInstructions.clear();
		continueInstructions.clear();
	}

	static void CompileBreak(const Statement& node, std::vector<Instruction>& instructions) {
		breakInstructions.push_back(instructions.size());

		Instruction jump{};
		jump.type = Instruction::Type::Jump;
		instructions.push_back(std::move(jump));
	}

	static void CompileContinue(const Statement& node, std::vector<Instruction>& instructions) {
		continueInstructions.push_back(instructions.size());

		Instruction jump{};
		jump.type = Instruction::Type::Jump;
		instructions.push_back(std::move(jump));
	}

	static void CompileReturn(const Statement& node, std::vector<Instruction>& instructions) {
		CompileExpression(node.expr, instructions);

		Instruction in{};
		in.type = Instruction::Type::Return;
		instructions.push_back(std::move(in));
	}

	static void CompileDef(const Statement& node, std::vector<Instruction>& instructions) {
		Instruction defName{};
		defName.type = Instruction::Type::Operation;
		defName.data.operation = new OperationInstructionInfo();
		defName.data.operation->op = Operation::Variable;
		defName.data.operation->token.literal.s = node.def.name;
		defName.data.operation->token.text = node.def.name;
		instructions.push_back(std::move(defName));

		Instruction def{};
		def.type = Instruction::Type::Def;
		def.data.def = new DefInstructionInfo();
		def.data.def->variables = std::vector<std::string>(
			node.def.variables.begin(),
			node.def.variables.end()
			);
		def.data.def->localCaptures = std::vector<std::string>(
			node.def.localCaptures.begin(),
			node.def.localCaptures.end()
			);
		def.data.def->globalCaptures = std::vector<std::string>(
			node.def.globalCaptures.begin(),
			node.def.globalCaptures.end()
			);
		def.data.def->parameters = node.def.parameters;
		instructions.push_back(std::move(def));

		Instruction assign{};
		assign.type = Instruction::Type::Operation;
		assign.data.operation = new OperationInstructionInfo();
		assign.data.operation->op = Operation::Assign;
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
