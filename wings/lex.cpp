#include "lex.h"
#include <regex>
#include <optional>

namespace wings {

	static const std::vector<std::string> SYMBOLS = {
		"(", ")", "[", "]", "{", "}", ":", ".", ",",
		"+", "-", "*", "**", "/", "//", "%",
		"<", ">", "<=", ">=", "==", "!=",
		"!", "&&", "||", "^", "&", "|", "~",
		"+=", "-=", "*=", "**=", "%=", "/=", "//=",
		">>=", "<<=", "|=", "&=", "^=", ";",
	};

	static std::string NormalizeLineEndings(const std::string& text) {
		auto s = std::regex_replace(text, std::regex("\r\n"), "\n");
		std::replace(s.begin(), s.end(), '\r', '\n');
		return s;
	}

	static bool IsAlpha(char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
	}

	static bool IsDigit(char c) {
		return c >= '0' && c <= '9';
	}

	static bool IsAlphaNum(char c) {
		return IsAlpha(c) || IsDigit(c);
	}

	static bool IsWhitespace(const std::string& s) {
		return s.find_first_not_of(" \t") == std::string::npos;
	}

	static bool IsWhitespaceChar(char c) {
		return c == ' ' || c == '\t';
	}

	static void StripComments(std::string& s) {
		s.erase(
			std::find(s.begin(), s.end(), '#'),
			s.end()
		);
	}

	static bool IsPossibleSymbol(const std::string& s) {
		return std::any_of(SYMBOLS.begin(), SYMBOLS.end(), [&](const auto& x) {return x.starts_with(s); });
	}

	static bool IsPossibleSymbol(char c) {
		return IsPossibleSymbol(std::string(1, c));
	}

	static std::vector<std::string> SplitLines(const std::string& s) {
		std::vector<std::string> v;
		size_t last = 0;
		size_t next = 0;
		while ((next = s.find('\n', last)) != std::string::npos) {
			v.push_back(s.substr(last, next - last));
			last = next + 1;
		}
		v.push_back(s.substr(last));
		return v;
	}

	static int IndentOf(const std::string& line, std::optional<std::string>& indentString, size_t& indent) {
		size_t i = 0;
		while (true) {
			// Reached end of line or comment before any code
			if (i >= line.size() || line[i] == '#') {
				indent = 0;
				return 0;
			}

			// Reached code
			if (line[i] != ' ' && line[i] != '\t')
				break;

			i++;
		}

		if (i == 0) {
			// No indent
			indent = 0;
			return 0;
		} else {
			// Make sure indent is either all spaces or all tabs
			if (!std::all_of(line.begin(), line.begin() + i, [&](char c) { return c == line[0]; })) {
				return -1;
			}

			if (!indentString.has_value()) {
				// Encountered first indent
				indentString = line.substr(0, i);
				indent = 1;
				return 0;
			} else {
				// Make sure indent is consistent with previous indents
				if (i % indentString.value().size()) {
					return -1;
				}

				indent = i / indentString.value().size();
				return 0;
			}
		}
	}

	static void RemoveEmptyLines(std::vector<std::string>& lines) {
		for (auto& line : lines)
			StripComments(line);

		lines.erase(
			std::remove_if(
				lines.begin(),
				lines.end(),
				IsWhitespace
			),
			lines.end()
		);
	}

	using StringIter = const char*;

	static Token ConsumeWord(StringIter& p) {
		Token t{};
		for (; *p && IsAlphaNum(*p); ++p) {
			t.text += *p;
		}
		t.type = Token::Type::Word;
		if (t.text == "None") {
			t.type = Token::Type::Null;
		} else if (t.text == "True" || t.text == "False") {
			t.type = Token::Type::Bool;
			t.literal.b = t.text[0] == 'T';
		}
		return t;
	}

	static LexError ConsumeNumber(StringIter& p, Token& out) {
		// TODO
		return LexError::Good();
	}

	static LexError ConsumeString(StringIter& p, Token& out) {
		char quote = *p;
		++p;

		Token t{};
		for (; *p && *p != quote; ++p) {
			t.text += *p;

			// Escape sequences TODO
			if (*p == '\\') {
				++p;
				if (*p == '\0') {
					return LexError::Bad("Missing closing quote");
				}

				char esc = 0;
				switch (*p) {
				case '0': esc = '\0'; break;
				case 'n': esc = '\n'; break;
				case 'r': esc = '\r'; break;
				case 't': esc = '\t'; break;
				case 'v': esc = '\v'; break;
				case 'b': esc = '\b'; break;
				case 'f': esc = '\f'; break;
				case '"': esc = '"'; break;
				case '\'': esc = '\''; break;
				case '\\': esc = '\\'; break;
				default: return LexError::Bad("Invalid escape sequence");
				}

				t.text += *p;
				t.literal.s += esc;
			} else {
				t.literal.s += *p;
			}
		}

		if (*p == '\0') {
			return LexError::Bad("Missing closing quote");
		}

		t.text = quote + t.text + quote;
		t.type = Token::Type::String;
		out = std::move(t);
		return LexError::Good();
	}

	static void ConsumeWhitespace(StringIter& p) {
		while (*p && IsWhitespaceChar(*p))
			++p;
	}

	static Token ConsumeSymbol(StringIter& p) {
		Token t{};
		for (; *p && IsPossibleSymbol(t.text + *p); ++p) {
			t.text += *p;
		}
		t.type = Token::Type::Symbol;
		return t;
	}

	static LexError TokenizeLine(const std::string& line, std::vector<Token>& out) {
		std::vector<Token> tokens;
		LexError error = LexError::Good();

		StringIter p = line.data();
		while (*p) {
			size_t srcColumn = p - line.data();
			bool wasWhitespace = false;

			if (IsAlpha(*p)) {
				tokens.push_back(ConsumeWord(p));
			} else if (IsDigit(*p)) {
				Token t{};
				if (!(error = ConsumeNumber(p, t))) {
					tokens.push_back(std::move(t));
				}
			} else if (*p == '\'' || *p == '"') {
				Token t{};
				if (!(error = ConsumeString(p, t))) {
					tokens.push_back(std::move(t));
				}
			} else if (IsPossibleSymbol(*p)) {
				tokens.push_back(ConsumeSymbol(p));
			} else if (IsWhitespaceChar(*p)) {
				ConsumeWhitespace(p);
				wasWhitespace = true;
			} else {
				error.good = false;
				error.srcPos.column = srcColumn;
				error.message = std::string("Unrecognised character ") + *p;
			}

			if (error) {
				out.clear();
				error.srcPos.column = srcColumn;
				return error;
			}

			if (!wasWhitespace) {
				tokens.back().srcPos.column = srcColumn;
			}
		}

		out = std::move(tokens);
		return LexError::Good();
	}

	// Returns [no. of open brackets] minus [no. close brackets]
	static int BracketBalance(std::vector<Token>& tokens) {
		int balance = 0;
		for (const auto& t : tokens) {
			if (t.text.size() == 1) {
				switch (t.text[0]) {
				case '(': case '[': case '{': balance++; break;
				case ')': case ']': case '}': balance--; break;
				}
			}
		}
		return balance;
	}

	static LexResult Lex(std::string code) {
		code = NormalizeLineEndings(code);
		auto rawCode = SplitLines(code);
		
		// Clean out comments and empty lines
		decltype(rawCode) lines = rawCode; // Using decltype to suppress copy warning
		RemoveEmptyLines(lines);

		// TODO

		LexResult result{};
		result.error.good = true;
		result.rawCode = std::move(rawCode);
		return result;
	}
}
