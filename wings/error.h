#pragma once
#include <stdint.h>
#include <string>

namespace wings {

	struct SourcePosition {
		size_t line = (size_t)-1;
		size_t column = (size_t)-1;
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
