#pragma once
#include <unordered_map>

struct WObj;

namespace wings {

	struct WObjHasher {
		size_t operator()(const WObj* obj) const;
	};

	struct WObjComparer {
		bool operator()(const WObj* lhs, const WObj* rhs) const;
	};

	struct WDict : public std::unordered_map<WObj*, WObj*, WObjHasher, WObjComparer> {};
}
