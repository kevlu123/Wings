#pragma once
#include "wings.h"
#include "rcptr.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdlib>

namespace wings {

	struct AttributeTable {
		AttributeTable();
		AttributeTable(const AttributeTable&) = delete;
		AttributeTable(AttributeTable&&) = default;
		AttributeTable& operator=(const AttributeTable&) = delete;
		AttributeTable& operator=(AttributeTable&&) = default;
		
		Wg_Obj* Get(const std::string& name) const;
		Wg_Obj* GetFromBase(const std::string& name) const;
		void Set(const std::string& name, Wg_Obj* value);
		
		void AddParent(AttributeTable& parent);
		AttributeTable Copy();
		template <class Fn> void ForEach(Fn fn) const;
	private:		
		struct Table {
			Wg_Obj* Get(const std::string& name) const;
			template <class Fn> void ForEach(Fn fn) const;
			std::unordered_map<std::string, Wg_Obj*> entries;
			std::vector<RcPtr<Table>> parents;
		};

		void Mutate();

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

