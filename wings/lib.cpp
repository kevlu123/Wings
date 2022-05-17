#include "impl.h"
#include <iostream>
#include <sstream>
#include <unordered_set>

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

	static void SetInvalidArgumentCountError(int expected, int given) {
		std::string msg = "function takes " +
			std::to_string(expected) +
			" argument(s) but " +
			std::to_string(given) +
			(given == 1 ? " was given" : " were given");
		WErrorSetRuntimeError(msg.c_str());
	}

	namespace attrlib {

		WObj* list_append(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(2, argc);
				return nullptr;
			} else if (!WObjIsList(argv[0])) {
				WErrorSetRuntimeError("function list.append() Argument 1 must be a list");
				return nullptr;
			}
			WObjListPush(argv[0], argv[1]);
			return WObjCreateNull(context);
		}

	}

	namespace lib {

		WObj* print(WObj** argv, int argc, WContext* context) {
			std::string text;
			for (int i = 0; i < argc; i++) {
				text += WObjToString(argv[i]);
				if (i < argc - 1)
					text += ' ';
			}
			text += '\n';
			std::cout << text;
			return WObjCreateNull(context);
		}

		WObj* str(WObj** argv, int argc, WContext* context) {
			if (argc != 1) {
				SetInvalidArgumentCountError(1, argc);
				return nullptr;
			}
			return WObjCreateString(context, WObjToString(argv[0]).c_str());
		}

		//WObj* range(WObj** argv, int argc, WContext* context) {
		//
		//}

		WObj* list(WObj** argv, int argc, WContext* context) {
			return WObjCreateList(context);
		}

	} // namespace lib

} // namespace wings

extern "C" {

	using namespace wings;

	bool WContextInitLibrary(WContext* context) {

		WFunc wfn{};
		WObj* obj{};

#define REGISTER_STATELESS_FUNCTION(name) \
		wfn = {}; \
		wfn.userdata = context; \
		wfn.fptr = [](WObj** argv, int argc, void* userdata) { return lib::name(argv, argc, (WContext*)userdata); }; \
		wfn.isMethod = false; \
		wfn.prettyName = #name; \
		WContextSetGlobal(context, #name, obj = WObjCreateFunc(context, &wfn))

#define REGISTER_STATELESS_METHOD(_class, name) \
		wfn = {}; \
		wfn.userdata = context; \
		wfn.fptr = [](WObj** argv, int argc, void* userdata) { return attrlib::_class ## _ ## name(argv, argc, (WContext*)userdata); }; \
		wfn.isMethod = true; \
		wfn.prettyName = #_class "." #name; \
		WObjSetAttribute(obj, #name, WObjCreateFunc(context, &wfn))

		REGISTER_STATELESS_FUNCTION(print);
		REGISTER_STATELESS_FUNCTION(str);

		REGISTER_STATELESS_FUNCTION(list);
		listClass = obj;
		REGISTER_STATELESS_METHOD(list, append);

		return true;
	}

} // extern "C"
