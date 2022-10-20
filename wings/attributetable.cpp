#include "attributetable.h"

namespace wings {

	AttributeTable::AttributeTable() :
		attributes(MakeRcPtr<Table>()),
		owned(true)
	{
	}

	Wg_Obj* AttributeTable::Get(const std::string& name) const {
		return attributes->Get(name);
	}

	Wg_Obj* AttributeTable::Table::Get(const std::string& name) const {
		auto it = entries.find(name);
		if (it != entries.end())
			return it->second;

		for (const auto& parent : parents)
			if (Wg_Obj* val = parent->Get(name))
				return val;

		return nullptr;
	}

	Wg_Obj* AttributeTable::GetFromBase(const std::string& name) const {
		for (const auto& parent : attributes->parents)
			if (Wg_Obj* val = parent->Get(name))
				return val;
		return nullptr;
	}

	void AttributeTable::Set(const std::string& name, Wg_Obj* value) {
		Mutate();
		attributes->entries[name] = value;
	}

	void AttributeTable::AddParent(AttributeTable& parent) {
		attributes->parents.push_back(parent.attributes);
	}

	AttributeTable AttributeTable::Copy() {
		AttributeTable copy;
		copy.attributes = attributes;
		copy.owned = false;
		return copy;
	}

	void AttributeTable::Mutate() {
		if (!owned) {
			attributes = MakeRcPtr<Table>(*attributes);
			owned = true;
		}
	}
}
