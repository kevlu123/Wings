#include "impl.h"
#include <iostream>
#include <sstream>
#include <unordered_set>

using namespace wings;

namespace wings {
	static std::string PtrToString(const void* p) {
		std::stringstream ss;
		ss << p;
		return ss.str();
	}

	static std::string WObjToString(const WObj* val, std::unordered_set<const WObj*>& seen) {
		switch (val->type) {
		case WObj::Type::Null:
			return "None";
		case WObj::Type::Bool:
			return val->b ? "True" : "False";
		case WObj::Type::Int:
			return std::to_string(val->i);
		case WObj::Type::Float: {
			std::string s = std::to_string(val->f);
			s.erase(s.find_last_not_of('0') + 1, std::string::npos);
			if (s.ends_with('.'))
				s.push_back('0');
			return s;
		}
		case WObj::Type::String:
			return val->s;
		case WObj::Type::Func:
			return "<function at " + PtrToString(val) + ">";
		case WObj::Type::Userdata:
			return "<userdata at " + PtrToString(val) + ">";
		case WObj::Type::List:
			if (seen.contains(val)) {
				return "[...]";
			} else {
				seen.insert(val);
				std::string s = "[";
				for (WObj* child : val->v) {;
					s += WObjToString(child, seen) + ", ";
				}
				if (!val->v.empty()) {
					s.pop_back();
					s.pop_back();
				}
				return s + "]";
			}
		case WObj::Type::Map:
			if (seen.contains(val)) {
				return "{...}";
			} else {
				seen.insert(val);
				std::string s = "{";
				for (const auto& [key, val] : val->m) {
					s += WObjToString(&key, seen) + ": ";
					s += WObjToString(val, seen) + ", ";
				}
				if (!val->m.empty()) {
					s.pop_back();
					s.pop_back();
				}
				return s + "}";
			}
		default:
			WUNREACHABLE();
		}
	}

	static std::string WObjToString(const WObj* val) {
		std::unordered_set<const WObj*> seen;
		return WObjToString(val, seen);
	}

} // namespace wings

extern "C" {

	void WContextInitLibrary(WContext* context) {
		WFunc print{};
		print.userdata = context;
		print.fptr = [](WObj** argv, int argc, void* userdata) {
			std::string text;
			for (int i = 0; i < argc; i++) {
				text += WObjToString(argv[i]);
				if (i < argc - 1)
					text += ' ';
			}
			text += '\n';
			std::cout << text;
			return WObjCreateNull((WContext*)userdata);
		};
		WContextSetGlobal(context, "print", WObjCreateFunc(context, &print));

		WFunc str{};
		str.userdata = context;
		str.fptr = [](WObj** argv, int argc, void* userdata) {
			if (argc != 1)
				return (WObj*)nullptr; // TODO: error message

			return WObjCreateString((WContext*)userdata, WObjToString(argv[0]).c_str());
		};
		WContextSetGlobal(context, "str", WObjCreateFunc(context, &str));
	}

} // extern "C"
