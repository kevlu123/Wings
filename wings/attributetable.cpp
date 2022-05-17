#include "attributetable.h"

namespace wings {

	AttributeTable::AttributeTable(Buffer&& attributes) :
		attributes(MakeRcPtr<Buffer>(std::move(attributes))),
		owned(true),
		referenced(false) {
	}

	WObj* AttributeTable::Get(const std::string& name) const {
		auto it = attributes->find(name);
		if (it == attributes->end()) {
			return nullptr;
		} else {
			return it->second;
		}
	}

	void AttributeTable::Set(const std::string& name, WObj* value) {
		if (referenced) {
			std::abort();
		}
		if (!owned) {
			attributes = MakeRcPtr<Buffer>(*attributes);
			owned = true;
		}
		attributes->insert({ name, value });
	}

	AttributeTable AttributeTable::Copy() {
		AttributeTable copy;
		copy.attributes = attributes;
		copy.owned = false;
		referenced = true;
		return copy;
	}

	AttributeTable::Buffer::const_iterator AttributeTable::begin() const {
		return attributes->begin();
	}

	AttributeTable::Buffer::const_iterator AttributeTable::end() const {
		return attributes->end();
	}
}
