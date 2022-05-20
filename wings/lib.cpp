#include "impl.h"
#include "gc.h"
#include <iostream>
#include <sstream>
#include <unordered_set>

using namespace wings;

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

using WFuncSignature = WObj * (*)(WObj**, int, WContext*);

template <WFuncSignature fn>
static WObj* RegisterStatelessFunction(WContext* context, const char* name) {
	WFunc wfn{};
	wfn.userdata = context;
	wfn.fptr = [](WObj** argv, int argc, void* userdata) { return fn(argv, argc, (WContext*)userdata); };
	wfn.isMethod = false;
	wfn.prettyName = name;

	WObj* obj = WObjCreateFunc(context, &wfn);
	WContextSetGlobal(context, name, obj);
	return obj;
}

template <WFuncSignature fn>
static WObj* RegisterStatelessMethod(WContext* context, AttributeTable& attributeTable, const char* name) {
	WFunc wfn{};
	wfn.userdata = context;
	wfn.fptr = [](WObj** argv, int argc, void* userdata) { return fn(argv, argc, (WContext*)userdata); };
	wfn.isMethod = true;
	wfn.prettyName = name;

	WObj* obj = WObjCreateFunc(context, &wfn);
	attributeTable.Set(name, obj);
	return obj;
}

namespace wings {

	namespace attrlib {

		static WObj* object_not_(WObj** argv, int argc, WContext* context) {
			if (argc != 1) {
				SetInvalidArgumentCountError(1, argc);
				return nullptr;
			}
			return WObjCreateBool(context, !WObjTruthy(argv[0]));
		}

		static WObj* object_and_(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(1, argc);
				return nullptr;
			}
			return WObjCreateBool(context, WObjTruthy(argv[0]) && WObjTruthy(argv[1]));
		}

		static WObj* object_eq_(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(1, argc);
				return nullptr;
			}
			return WObjCreateBool(context, WObjEquals(argv[0], argv[1]));
		}

		static WObj* object_str_(WObj** argv, int argc, WContext* context) {
			if (argc != 1) {
				SetInvalidArgumentCountError(1, argc);
				return nullptr;
			}
			return WObjCreateString(context, WObjToString(argv[0]).c_str());
		}

		static WObj* int_mod_(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(2, argc);
				return nullptr;
			}
			return WObjCreateInt(context, WObjGetInt(argv[0]) % WObjGetInt(argv[1]));
		}

		static WObj* list_append(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(2, argc);
				return nullptr;
			} else if (!WObjIsList(argv[0])) {
				WErrorSetRuntimeError("method list.append() Argument 1 must be a list");
				return nullptr;
			}
			WObjListPush(argv[0], argv[1]);
			return WObjCreateNull(context);
		}

	} // namespace attrlib

	namespace lib {

		static WObj* object(WObj** argv, int argc, WContext* context) {
			return WObjCreateObject(context);
		}

		static WObj* _bool(WObj** argv, int argc, WContext* context) {
			return WObjCreateBool(context);
		}

		static WObj* _int(WObj** argv, int argc, WContext* context) {
			return WObjCreateInt(context);
		}

		static WObj* _float(WObj** argv, int argc, WContext* context) {
			return WObjCreateFloat(context);
		}

		static WObj* str(WObj** argv, int argc, WContext* context) {
			switch (argc) {
			case 0:
				return WObjCreateString(context);
			case 1: {
				WObj* method = WObjGetAttribute(argv[0], "__str__");
				return WObjCall(method, nullptr, 0);
			}
			default:
				SetInvalidArgumentCountError(1, argc);
				return nullptr;
			}
		}

		static WObj* list(WObj** argv, int argc, WContext* context) {
			return WObjCreateList(context);
		}

		static WObj* dict(WObj** argv, int argc, WContext* context) {
			return WObjCreateMap(context);
		}

		static WObj* print(WObj** argv, int argc, WContext* context) {
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

		static WObj* range(WObj** argv, int argc, WContext* context) {
			if (argc < 1) {
				SetInvalidArgumentCountError(1, argc);
				return nullptr;
			} else if (argc > 3) {
				SetInvalidArgumentCountError(3, argc);
				return nullptr;
			}

			WObj* rangeObj = WObjCreateObject(context);
			WGcProtect(rangeObj);
			switch (argc) {
			case 1:
				WObjSetAttribute(rangeObj, "_cur", WObjCreateInt(context, 0));
				WObjSetAttribute(rangeObj, "_end", argv[0]);
				WObjSetAttribute(rangeObj, "_step", WObjCreateInt(context, 1));
				break;
			case 2:
				WObjSetAttribute(rangeObj, "_cur", argv[0]);
				WObjSetAttribute(rangeObj, "_end", argv[1]);
				WObjSetAttribute(rangeObj, "_step", WObjCreateInt(context, 1));
				break;
			case 3:
				WObjSetAttribute(rangeObj, "_cur", argv[0]);
				WObjSetAttribute(rangeObj, "_end", argv[1]);
				WObjSetAttribute(rangeObj, "_step", argv[2]);
				break;
			}

			auto iterend = [](WObj** argv, int argc, WContext* context) {
				WObj* cur = WObjGetAttribute(argv[0], "_cur");
				WObj* end = WObjGetAttribute(argv[0], "_end");
				WObj* step = WObjGetAttribute(argv[0], "_step");
				if (WObjGetInt(step) > 0) {
					return WObjCreateBool(context, WObjGetInt(cur) >= WObjGetInt(end));
				} else {
					return WObjCreateBool(context, WObjGetInt(cur) <= WObjGetInt(end));
				}
			};
			RegisterStatelessMethod<iterend>(context, rangeObj->attributes, "__iterend__");

			auto next = [](WObj** argv, int argc, WContext* context) {
				WObj* cur = WObjGetAttribute(argv[0], "_cur");
				WObj* end = WObjGetAttribute(argv[0], "_end");
				WObj* step = WObjGetAttribute(argv[0], "_step");

				int nextVal = WObjGetInt(cur) + WObjGetInt(step);
				WObjSetAttribute(argv[0], "_cur", WObjCreateInt(context, nextVal));
				return cur;
			};
			RegisterStatelessMethod<next>(context, rangeObj->attributes, "__iternext__");

			WGcUnprotect(rangeObj);
			return rangeObj;
		}

	} // namespace lib

	bool InitLibrary(WContext* context) {

		RegisterStatelessFunction<lib::object>(context, "object");
		RegisterStatelessMethod<attrlib::object_not_>(context, context->attributeTables.object, "__not__");
		RegisterStatelessMethod<attrlib::object_and_>(context, context->attributeTables.object, "__and__");
		RegisterStatelessMethod<attrlib::object_eq_>(context, context->attributeTables.object, "__eq__");
		RegisterStatelessMethod<attrlib::object_str_>(context, context->attributeTables.object, "__str__");

		context->attributeTables.null.SetSuper(context->attributeTables.object);
		context->attributeTables._bool.SetSuper(context->attributeTables.object);
		context->attributeTables._int.SetSuper(context->attributeTables.object);
		context->attributeTables._float.SetSuper(context->attributeTables.object);
		context->attributeTables.str.SetSuper(context->attributeTables.object);
		context->attributeTables.list.SetSuper(context->attributeTables.object);
		context->attributeTables.map.SetSuper(context->attributeTables.object);
		context->attributeTables.func.SetSuper(context->attributeTables.object, false);
		context->attributeTables.userdata.SetSuper(context->attributeTables.object);

		RegisterStatelessFunction<lib::_bool>(context, "bool");

		RegisterStatelessFunction<lib::_int>(context, "int");
		RegisterStatelessMethod<attrlib::int_mod_>(context, context->attributeTables._int, "__mod__");

		RegisterStatelessFunction<lib::_float>(context, "float");

		RegisterStatelessFunction<lib::str>(context, "str");

		RegisterStatelessFunction<lib::list>(context, "list");
		RegisterStatelessMethod<attrlib::list_append>(context, context->attributeTables.list, "append");

		RegisterStatelessFunction<lib::dict>(context, "dict");


		RegisterStatelessFunction<lib::print>(context, "print");
		RegisterStatelessFunction<lib::range>(context, "range");

		return true;
	}

} // namespace wings
