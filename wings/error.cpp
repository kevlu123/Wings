#include "error.h"

namespace wings {

	std::string CodeError::ToString() const {
		if (good) {
			return "Success";
		} else {
			return '(' + std::to_string(srcPos.line + 1) + ','
				+ std::to_string(srcPos.column + 1) + ") "
				+ message;
		}
	}

	CodeError::operator bool() const {
		return !good;
	}

	CodeError CodeError::Good() {
		return CodeError{ true };
	}

	CodeError CodeError::Bad(std::string message, SourcePosition srcPos) {
		return CodeError{
			.good = false,
			.srcPos = srcPos,
			.message = message
		};
	}
}
