#pragma once
#include <string>
#include <unordered_map>
#include <cstdlib>
#include "rcptr.h"

struct Wg_Obj;

namespace wings {

	struct AttributeTable {
		AttributeTable();
		Wg_Obj* Get(const std::string& name) const;
		void Set(const std::string& name, Wg_Obj* value);
		Wg_Obj* GetFromBase(const std::string& name) const;
		void AddParent(AttributeTable& parent);
		AttributeTable Copy();
		bool Empty() const;
		template <class Fn> void ForEach(Fn fn) const;
	private:
		struct Table {
			std::unordered_map<std::string, Wg_Obj*> entries;
			std::vector<RcPtr<Table>> parents;

			Wg_Obj* Get(const std::string& name) const;
			template <class Fn> void ForEach(Fn fn) const;
		};

		RcPtr<Table> attributes;
		bool owned;
	};

	template <class Fn>
	void AttributeTable::ForEach(Fn fn) const {
		attributes->ForEach(fn);
	}

	template <class Fn>
	void AttributeTable::Table::ForEach(Fn fn) const {
		for (const auto& [_, val] : entries)
			fn(val);

		for (const auto& parent : parents)
			parent->ForEach(fn);
	}
}

