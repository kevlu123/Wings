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
		WObj* GetFromBase(const std::string& name) const;
		void AddParent(AttributeTable& parent, bool validate = true);
		AttributeTable Copy();
		bool Empty() const;
		template <class Fn> void ForEach(Fn fn) const;
	private:
		struct Table {
			std::unordered_map<std::string, WObj*> entries;
			std::vector<RcPtr<Table>> parents;

			WObj* Get(const std::string& name) const;
			template <class Fn> void ForEach(Fn fn) const;
		};

		RcPtr<Table> attributes;
		bool owned;
		bool referenced;
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

