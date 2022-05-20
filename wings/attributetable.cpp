#include "attributetable.h"

namespace wings {

	AttributeTable::AttributeTable() :
		attributes(MakeRcPtr<Table>()),
		owned(true),
		referenced(false) {
	}

	WObj* AttributeTable::Get(const std::string& name) const {
		for (auto* table = &attributes; *table; table = &(*table)->super) {
			auto it = (*table)->entries.find(name);
			if (it != (*table)->entries.end()) {
				return it->second;
			}
		}
		return nullptr;
	}

	void AttributeTable::Set(const std::string& name, WObj* value, bool validate) {
		if (validate && referenced) {
			std::abort();
		}

		if (!owned) {
			attributes = MakeRcPtr<Table>(*attributes);
			owned = true;
		}

		for (auto* table = &attributes; *table; table = &(*table)->super) {
			if ((*table)->entries.contains(name)) {
				(*table)->entries[name] = value;
				return;
			}
		}
		attributes->entries.insert({ name, value });
	}

	void AttributeTable::SetSuper(AttributeTable& super, bool validate) {
		if (validate && referenced) {
			std::abort();
		}

		attributes->super = super.attributes;
		super.referenced = true;
	}

	AttributeTable AttributeTable::Copy() {
		AttributeTable copy;
		copy.attributes = attributes;
		copy.owned = false;
		referenced = true;
		return copy;
	}
}
