#pragma once
#include <unordered_map>
#include <unordered_set>

struct WObj;

namespace wings {

	struct WObjHasher {
		size_t operator()(const WObj* obj) const;
	};

	struct WObjComparer {
		bool operator()(const WObj* lhs, const WObj* rhs) const;
	};

	struct WDict : public std::unordered_map<WObj*, WObj*, WObjHasher, WObjComparer> {
	};

	struct WSet : public std::unordered_set<WObj*, WObjHasher, WObjComparer> {
	};
}
