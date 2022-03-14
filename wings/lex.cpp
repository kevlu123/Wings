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
		return s.find_first_not_of(' ') == std::string::npos;
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
}
