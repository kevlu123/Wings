#pragma once
#include <vector>
#include <string>
#include "wings.h"

namespace wings {

	struct SourcePosition {
		size_t line;
		size_t column;
	};

	struct Token {
		enum class Type {
			Bool,
			Int,
			Float,
			Str,
			Symbol,
			Word,
		} type;

		std::string text;
		SourcePosition srcPos;

		struct {
			union {
				bool b;
				int i = 0;
				wfloat f;
			};
			std::string s;
		} literal;
	};

	struct LexTree {
		std::vector<Token> tokens;
		std::vector<LexTree> children;
	};

	struct LexResult {
		std::vector<std::string> rawCode;
		bool success{};
		LexTree lexTree;
		struct {
			SourcePosition srcPos{};
			std::string message;
		} error;
	};

	LexResult Lex(std::string code);

}
