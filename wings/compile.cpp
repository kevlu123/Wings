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

	static void CompileExpression(const Expression& expression, std::vector<Instruction>& instructions) {
		for (const auto& child : expression.children) {
			CompileExpression(child, instructions);
		}

		Instruction instr;
		instr.type = Instruction::Type::Operation;
		instr.data.operation = new OperationInstructionInfo;
		instr.data.operation->op = expression.operation;
		instr.data.operation->token = expression.literal;
		instructions.push_back(std::move(instr));
	}

	static void CompileExpressionStatement(const Statement& node, std::vector<Instruction>& instructions) {
		CompileExpression(node.expr, instructions);
	}

	static void CompileIf(const Statement& node, std::vector<Instruction>& instructions) {}
	static void CompileElse(const Statement& node, std::vector<Instruction>& instructions) {}
	static void CompileWhile(const Statement& node, std::vector<Instruction>& instructions) {}
	static void CompileFor(const Statement& node, std::vector<Instruction>& instructions) {}
	static void CompileBreak(const Statement& node, std::vector<Instruction>& instructions) {}
	static void CompileContinue(const Statement& node, std::vector<Instruction>& instructions) {}

	static void CompileReturn(const Statement& node, std::vector<Instruction>& instructions) {
		CompileExpression(node.expr, instructions);

		Instruction in;
		in.type = Instruction::Type::Return;
		instructions.push_back(std::move(in));
	}

	static void CompileDef(const Statement& node, std::vector<Instruction>& instructions) {}

	using CompileFn = void(*)(const Statement&, std::vector<Instruction>&);

	static const std::unordered_map<Statement::Type, CompileFn> COMPILE_FUNCTIONS = {
		{ Statement::Type::Expr, CompileExpressionStatement },
		{ Statement::Type::If, CompileIf },
		{ Statement::Type::Else, CompileElse },
		{ Statement::Type::While, CompileWhile },
		{ Statement::Type::For, CompileFor },
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
		std::vector<Instruction> instructions;
		CompileBody(parseTree, instructions);
		return instructions;
	}

}
