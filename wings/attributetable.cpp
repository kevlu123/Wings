#include "attributetable.h"

namespace wings {

	AttributeTable::AttributeTable() :
		attributes(MakeRcPtr<Table>()),
		owned(true),
		referenced(false) {
	}

	WObj* AttributeTable::Get(const std::string& name) const {
		return attributes->Get(name);
	}

	WObj* AttributeTable::Table::Get(const std::string& name) const {
		auto it = entries.find(name);
		if (it != entries.end())
			return it->second;

		for (const auto& parent : parents)
			if (WObj* val = parent->Get(name))
				return val;

		return nullptr;
	}

	WObj* AttributeTable::GetFromBase(const std::string& name) const {
		for (const auto& parent : attributes->parents)
			if (WObj* val = parent->Get(name))
				return val;
		return nullptr;
	}

	void AttributeTable::Set(const std::string& name, WObj* value, bool validate) {
		if (!owned) {
			attributes = MakeRcPtr<Table>(*attributes);
			owned = true;
		}

		attributes->entries[name] = value;
	}

	void AttributeTable::AddParent(AttributeTable& parent, bool validate) {
		attributes->parents.push_back(parent.attributes);
		parent.referenced = true;
	}

	AttributeTable AttributeTable::Copy() {
		AttributeTable copy;
		copy.attributes = attributes;
		copy.owned = false;
		referenced = true;
		return copy;
	}

	bool AttributeTable::Empty() const {
		return attributes->entries.empty() && attributes->parents.empty();
	}
}
