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
			Null,
			Bool,
			Int,
			Float,
			String,
			Symbol,
			Word,
		} type;

		std::string text;
		SourcePosition srcPos;

		struct {
			union {
				bool b;
				int i;
				wfloat f;
			};
			std::string s;
		} literal;

		std::string ToString() const;
	};

	struct LexTree {
		std::vector<Token> tokens;
		std::vector<LexTree> children;
	};

	struct LexError {
		bool good{};
		SourcePosition srcPos{};
		std::string message;

		operator bool() const;
		std::string ToString() const;
		static LexError Good();
		static LexError Bad(std::string message);
	};

	struct LexResult {
		std::vector<std::string> rawCode;
		LexTree lexTree; // Root tree contains no tokens
		LexError error;
	};

	LexResult Lex(std::string code);

}
