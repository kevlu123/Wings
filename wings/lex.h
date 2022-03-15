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
	};

	struct LexTree {
		std::vector<Token> tokens;
		std::vector<LexTree> children;
	};

	struct LexError {
		bool good{};
		SourcePosition srcPos{};
		std::string message;
		operator bool() const { return !good; }
		static LexError Good() { return LexError{ true }; }
		static LexError Bad(std::string message) {
			return LexError{
				.good = false,
				.srcPos = {},
				.message = message
			};
		}
	};

	struct LexResult {
		std::vector<std::string> rawCode;
		LexTree lexTree;
		LexError error;
	};

	LexResult Lex(std::string code);

}
