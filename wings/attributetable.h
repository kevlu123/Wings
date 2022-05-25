#pragma once
#include <string>
#include <unordered_map>
#include <cstdlib>
#include "rcptr.h"

struct WObj;

namespace wings {

	struct AttributeTable {
		AttributeTable();
		WObj* Get(const std::string& name) const;
		void Set(const std::string& name, WObj* value, bool validate = true);
		void SetSuper(AttributeTable& super, bool validate = true);
		AttributeTable Copy();
		bool Empty() const;
		template <class Fn> void ForEach(Fn fn) const;
	private:
		struct Table {
			std::unordered_map<std::string, WObj*> entries;
			RcPtr<Table> super;
		};

		RcPtr<Table> attributes;
		bool owned;
		bool referenced;
	};

	template <class Fn>
	void AttributeTable::ForEach(Fn fn) const {
		for (auto* table = &attributes; *table; table = &(*table)->super) {
			for (const auto& entry : (*table)->entries) {
				fn(entry);
			}
		}
	}
}

