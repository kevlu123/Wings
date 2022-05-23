#include "impl.h"
#include "gc.h"
#include <iostream>
#include <sstream>
#include <unordered_set>
#include <cmath>

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

static std::string WObjTypeToString(WObj::Type t) {
	switch (t) {
	case WObj::Type::Null: return "NoneType";
	case WObj::Type::Bool: return "bool";
	case WObj::Type::Int: return "int";
	case WObj::Type::Float: return "float";
	case WObj::Type::String: return "str";
	case WObj::Type::List: return "list";
	case WObj::Type::Map: return "map";
	case WObj::Type::Object: return "object";
	case WObj::Type::Func: return "function";
	case WObj::Type::Userdata: return "userdata";
	default: WUNREACHABLE();
	}
}

static void SetInvalidArgumentCountError(WContext* context, const std::string& fnName, int given, int expected = -1) {
	std::string msg;
	if (expected != -1) {
		msg = "function " +
			fnName +
			"() takes " +
			std::to_string(expected) +
			" argument(s) but " +
			std::to_string(given) +
			(given == 1 ? " was given" : " were given");
	} else {
		msg = "function " +
			fnName +
			"() does not take " +
			std::to_string(given) +
			" argument(s)";
	}
	WErrorSetRuntimeError(context, msg.c_str());
}

static void SetInvalidTypeError(WContext* context, const std::string& fnName, int paramNumber, const std::string& expectedType, WObj* given) {
	std::string msg = "function " +
		fnName +
		"() expected type " +
		expectedType +
		" on argument " +
		std::to_string(paramNumber) +
		" but got " +
		WObjTypeToString(given->type);
	WErrorSetRuntimeError(context, msg.c_str());
}

static void SetMissingAttributeError(WContext* context, const std::string& fnName, int paramNumber, const std::string& attribute, WObj* given) {
	std::string msg = "function " +
		fnName +
		"() argument " +
		std::to_string(paramNumber) +
		" (of type " +
		WObjTypeToString(given->type) +
		") has no attribute " +
		attribute;
	WErrorSetRuntimeError(context, msg.c_str());
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
	attributeTable.Set(name, obj, false);
	return obj;
}

template <WFuncSignature Constructor>
static WObj* CreateClass(WContext* context, const char* name = nullptr) {
	WObj* _class = Alloc(context);
	if (_class == nullptr) {
		return nullptr;
	}

	WFunc constructor{};
	constructor.userdata = _class;
	constructor.isMethod = false;
	constructor.prettyName = name;

	constructor.fptr = [](WObj** argv, int argc, void* userdata) {
		WObj* _class = (WObj*)userdata;
		WObj* instance = Constructor(argv, argc, _class->context);
		if (instance == nullptr)
			return (WObj*)nullptr;
		instance->attributes = _class->c.Copy();
		return instance;
	};

	_class->type = WObj::Type::Class;
	_class->fn = constructor;

	if (name) {
		WContextSetGlobal(context, name, _class);
	}

	return _class;
}

namespace wings {

	namespace classlib {

		static WObj* null(WObj** argv, int argc, WContext* context) {
			// Not callable from user code
			WASSERT(argc == 0);

			WObj* obj = Alloc(context);
			if (obj == nullptr)
				return nullptr;
			obj->type = WObj::Type::Null;
			return obj;
		}

		static WObj* _bool(WObj** argv, int argc, WContext* context) {
			bool b{};
			switch (argc) {
			case 0:
				b = false;
				break;
			case 1:
				b = WObjTruthy(argv[0]);
				break;
			default:
				SetInvalidArgumentCountError(context, "bool", argc);
				return nullptr;
			}

			WObj* obj = Alloc(context);
			if (obj == nullptr)
				return nullptr;
			obj->type = WObj::Type::Bool;
			obj->b = b;
			return obj;
		}

		static WObj* _int(WObj** argv, int argc, WContext* context) {
			int i{};
			switch (argc) {
			case 0:
				i = 0;
				break;
			//case 1:
			//	break;
			default:
				SetInvalidArgumentCountError(context, "int", argc);
				return nullptr;
			}

			WObj* obj = Alloc(context);
			if (obj == nullptr)
				return nullptr;
			obj->type = WObj::Type::Int;
			obj->i = i;
			return obj;
		}

		static WObj* _float(WObj** argv, int argc, WContext* context) {
			wfloat f{};
			switch (argc) {
			case 0:
				f = 0;
				break;
				// TODO
				//case 1:
				//	break;
			default:
				SetInvalidArgumentCountError(context, "float", argc);
				return nullptr;
			}

			WObj* obj = Alloc(context);
			if (obj == nullptr)
				return nullptr;
			obj->type = WObj::Type::Float;
			obj->f = f;
			return obj;
		}

		static WObj* str(WObj** argv, int argc, WContext* context) {
			switch (argc) {
			case 0: {
				WObj* obj = Alloc(context);
				if (obj == nullptr)
					return nullptr;
				obj->type = WObj::Type::String;
				return obj;
			}
			case 1:
				if (WObj* method = WObjGetAttribute(argv[0], "__str__")) {
					return WObjCall(method, nullptr, 0);
				} else {
					SetMissingAttributeError(context, "str", 1, "__str__", argv[0]);
					return nullptr;
				}
			default:
				SetInvalidArgumentCountError(context, "str", argc);
				return nullptr;
			}
		}

		static WObj* list(WObj** argv, int argc, WContext* context) {
			// TODO: validate params

			WObj* obj = Alloc(context);
			if (obj == nullptr)
				return nullptr;
			obj->type = WObj::Type::List;
			return obj;
		}

		static WObj* map(WObj** argv, int argc, WContext* context) {
			// TODO: validate params

			WObj* obj = Alloc(context);
			if (obj == nullptr)
				return nullptr;
			obj->type = WObj::Type::Map;
			return obj;
		}

		static WObj* func(WObj** argv, int argc, WContext* context) {
			// Not callable from user code
			WObj* obj = Alloc(context);
			if (obj == nullptr)
				return nullptr;
			obj->type = WObj::Type::Func;
			return obj;
		}

		static WObj* object(WObj** argv, int argc, WContext* context) {
			if (argc != 0) {
				SetInvalidArgumentCountError(context, "object", argc, 0);
				return nullptr;
			}

			WObj* obj = Alloc(context);
			if (obj == nullptr)
				return nullptr;
			obj->type = WObj::Type::Object;
			return obj;
		}

		static WObj* userdata(WObj** argv, int argc, WContext* context) {
			// Not callable from user code
			WObj* obj = Alloc(context);
			if (obj == nullptr)
				return nullptr;
			obj->type = WObj::Type::Userdata;
			return obj;
		}

	}

	namespace attrlib {

		static WObj* object_not_(WObj** argv, int argc, WContext* context) {
			if (argc != 1) {
				SetInvalidArgumentCountError(context, "object.__not__", 1, argc);
				return nullptr;
			}
			return WObjCreateBool(context, !WObjTruthy(argv[0]));
		}

		static WObj* object_and_(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "object.__and__", 1, argc);
				return nullptr;
			}
			return WObjCreateBool(context, WObjTruthy(argv[0]) && WObjTruthy(argv[1]));
		}

		static WObj* object_eq_(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "object.__eq__", 1, argc);
				return nullptr;
			}
			return WObjCreateBool(context, WObjEquals(argv[0], argv[1]));
		}

		static WObj* object_str_(WObj** argv, int argc, WContext* context) {
			if (argc != 1) {
				SetInvalidArgumentCountError(context, "object.__str__", argc, 1);
				return nullptr;
			}
			return WObjCreateString(context, WObjToString(argv[0]).c_str());
		}

		static WObj* int_mod_(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "int.__mod__", 2, argc);
				return nullptr;
			} else if (!WObjIsInt(argv[0])) {
				SetInvalidTypeError(context, "int.__mod__", 0, "int", argv[0]);
				return nullptr;
			}

			switch (argv[1]->type) {
			case WObj::Type::Int: {
				int mod = WObjGetInt(argv[1]);
				int result = WObjGetInt(argv[0]) % mod;
				if (result < 0)
					result += mod;
				return WObjCreateInt(context, result);
			}
			case WObj::Type::Float: {
				float mod = WObjGetFloat(argv[1]);
				float result = std::fmodf(WObjGetFloat(argv[0]), mod);
				if (result < 0)
					result += mod;
				return WObjCreateFloat(context, result);
			}
			default:
				SetInvalidTypeError(context, "int.__mod__", 1, "int", argv[1]);
				return nullptr;
			}
		}

		static WObj* list_append(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "list.append", 2, argc);
				return nullptr;
			} else if (!WObjIsList(argv[0])) {
				SetInvalidTypeError(context, "list.append", 1, "list", argv[0]);
				return nullptr;
			}

			WObjListPush(argv[0], argv[1]);
			return WObjCreateNull(context);
		}

	} // namespace attrlib

	namespace lib {

		static WObj* print(WObj** argv, int argc, WContext* context) {
			std::string text;
			for (int i = 0; i < argc; i++) {
				if (WObj* method = WObjGetAttribute(argv[i], "__str__")) {
					WObj* s = WObjCall(method, nullptr, 0);
					if (s == nullptr) {
						return nullptr;
					} else if (!WObjIsString(s)) {
						WErrorSetRuntimeError(context, ("function print() __str__ returned a non-string type on argument " + std::to_string(i + 1)).c_str());
						return nullptr;
					} else {
						text += WObjGetString(s);
					}
				} else {
					SetMissingAttributeError(context, "print", i + 1, "__str__", argv[i]);
					return nullptr;
				}

				if (i < argc - 1)
					text += ' ';
			}
			text += '\n';
			std::cout << text;
			return WObjCreateNull(context);
		}

		static WObj* range(WObj** argv, int argc, WContext* context) {
			if (argc < 1) {
				SetInvalidArgumentCountError(context, "range", 1, argc);
				return nullptr;
			} else if (argc > 3) {
				SetInvalidArgumentCountError(context, "range", 3, argc);
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

		// Returns from function if operation resulted in nullptr
#define CheckOperation(op) do { if ((op) == nullptr) return false; } while (0)

		// Create builtin classes
		CheckOperation(context->builtinClasses.null = CreateClass<classlib::null>(context));
		CheckOperation(context->builtinClasses._bool = CreateClass<classlib::_bool>(context, "bool"));
		CheckOperation(context->builtinClasses._int = CreateClass<classlib::_int>(context, "int"));
		CheckOperation(context->builtinClasses._float = CreateClass<classlib::_float>(context, "float"));
		CheckOperation(context->builtinClasses.str = CreateClass<classlib::str>(context, "str"));
		CheckOperation(context->builtinClasses.list = CreateClass<classlib::list>(context, "list"));
		CheckOperation(context->builtinClasses.map = CreateClass<classlib::map>(context, "dict"));
		CheckOperation(context->builtinClasses.func = CreateClass<classlib::func>(context));
		CheckOperation(context->builtinClasses.object = CreateClass<classlib::object>(context, "object"));
		CheckOperation(context->builtinClasses.userdata = CreateClass<classlib::userdata>(context));

		// Subclass the object class
		AttributeTable& objectAttributes = context->builtinClasses.object->attributes;
		context->builtinClasses.null->c.SetSuper(objectAttributes);
		context->builtinClasses._bool->c.SetSuper(objectAttributes);
		context->builtinClasses._int->c.SetSuper(objectAttributes);
		context->builtinClasses._float->c.SetSuper(objectAttributes);
		context->builtinClasses.str->c.SetSuper(objectAttributes);
		context->builtinClasses.list->c.SetSuper(objectAttributes);
		context->builtinClasses.map->c.SetSuper(objectAttributes);
		context->builtinClasses.func->c.SetSuper(objectAttributes);
		context->builtinClasses.userdata->c.SetSuper(objectAttributes);

		// Create null (None) singleton
		CheckOperation(context->nullSingleton = WObjCall(context->builtinClasses.null, nullptr, 0));

		// Register methods of builtin classes
		CheckOperation(RegisterStatelessMethod<attrlib::object_not_>(context, objectAttributes, "__not__"));
		CheckOperation(RegisterStatelessMethod<attrlib::object_and_>(context, objectAttributes, "__and__"));
		CheckOperation(RegisterStatelessMethod<attrlib::object_eq_>(context, objectAttributes, "__eq__"));
		CheckOperation(RegisterStatelessMethod<attrlib::object_str_>(context, objectAttributes, "__str__"));

		CheckOperation(RegisterStatelessMethod<attrlib::int_mod_>(context, context->builtinClasses._int->attributes, "__mod__"));

		CheckOperation(RegisterStatelessMethod<attrlib::list_append>(context, context->builtinClasses.list->attributes, "append"));

		// Register builtin functions
		CheckOperation(RegisterStatelessFunction<lib::print>(context, "print"));
		CheckOperation(RegisterStatelessFunction<lib::range>(context, "range"));

		return true;
	}

} // namespace wings
