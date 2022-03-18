#pragma once
#include <stdint.h>
#include <string>

namespace wings {

	struct SourcePosition {
		size_t line;
		size_t column;
	};

	struct CodeError {
		bool good = true;
		SourcePosition srcPos{};
		std::string message;

		operator bool() const;
		std::string ToString() const;
		static CodeError Good();
		static CodeError Bad(std::string message, SourcePosition srcPos = {});
	};

}
