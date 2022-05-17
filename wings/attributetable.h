#pragma once
#include <string>
#include <unordered_map>
#include <cstdlib>
#include "rcptr.h"

struct WObj;

namespace wings {
	struct AttributeTable {
		using Buffer = std::unordered_map<std::string, WObj*>;
		AttributeTable(Buffer&& attributes = {});
		WObj* Get(const std::string& name) const;
		void Set(const std::string& name, WObj* value);
		AttributeTable Copy();
		Buffer::const_iterator begin() const;
		Buffer::const_iterator end() const;
	private:
		RcPtr<Buffer> attributes;
		bool owned;
		bool referenced;
	};
}
