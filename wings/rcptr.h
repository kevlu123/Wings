#pragma once
#include <memory>

namespace wings {

	// Definitions may change later

	// Reference counted smart pointer.
	template <typename T>
	using RcPtr = std::shared_ptr<T>;

	template <typename T, typename... Args>
	RcPtr<T> MakeRcPtr(Args&&... args) {
		return std::make_shared<T>(std::forward<Args>(args)...);
	}
}
