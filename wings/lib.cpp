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

static size_t ConvertNegativeIndex(wint index, size_t size) {
	if (index < 0) {
		return size + index;
	} else {
		return index;
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

static void SetIndexOutOfRangeError(WContext* context, const std::string& fnName, int paramNumber) {
	std::string msg = "function " +
		fnName +
		"() argument" +
		std::to_string(paramNumber) +
		" index out of range";
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
		if (instance->attributes.Empty())
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

		static WObj* Null(WObj** argv, int argc, WContext* context) {
			// Not callable from user code
			WASSERT(argc == 0);

			WObj* obj = Alloc(context);
			if (obj == nullptr)
				return nullptr;
			obj->type = WObj::Type::Null;
			return obj;
		}

		static WObj* Bool(WObj** argv, int argc, WContext* context) {
			switch (argc) {
			case 0:
				if (WObj* obj = Alloc(context)) {
					obj->type = WObj::Type::Bool;
					obj->b = false;
					return obj;
				} else {
					return nullptr;
				}
			case 1:
				if (WObj* res = WOpTruthy(argv[0])) {
					if (WObjIsBool(res)) {
						return res;
					} else {
						WErrorSetRuntimeError(context, "function bool() argument 1 __nonzero__() method returned a non bool type.");
					}
				}
				return nullptr;
			default:
				SetInvalidArgumentCountError(context, "bool", argc);
				return nullptr;
			}
		}

		static WObj* Int(WObj** argv, int argc, WContext* context) {
			switch (argc) {
			case 0:
				if (WObj* obj = Alloc(context)) {
					obj->type = WObj::Type::Int;
					obj->i = 0;
					return obj;
				} else {
					return nullptr;
				}
			case 1:
				return WOpCastToInt(argv[0]);
			default:
				SetInvalidArgumentCountError(context, "int", argc);
				return nullptr;
			}
		}

		static WObj* Float(WObj** argv, int argc, WContext* context) {
			switch (argc) {
			case 0:
				if (WObj* obj = Alloc(context)) {
					obj->type = WObj::Type::Float;
					obj->f = 0;
					return obj;
				} else {
					return nullptr;
				}
			case 1:
				return WOpCastToFloat(argv[0]);
			default:
				SetInvalidArgumentCountError(context, "float", argc);
				return nullptr;
			}
		}

		static WObj* Str(WObj** argv, int argc, WContext* context) {
			switch (argc) {
			case 0:
				if (WObj* obj = Alloc(context)) {
					obj->type = WObj::Type::String;
					return obj;
				} else {
					return nullptr;
				}
			case 1:
				return WOpCastToString(argv[0]);
			default:
				SetInvalidArgumentCountError(context, "str", argc);
				return nullptr;
			}
		}

		static WObj* List(WObj** argv, int argc, WContext* context) {
			// TODO: validate params

			WObj* obj = Alloc(context);
			if (obj == nullptr)
				return nullptr;
			obj->type = WObj::Type::List;
			return obj;
		}

		static WObj* Map(WObj** argv, int argc, WContext* context) {
			// TODO: validate params

			WObj* obj = Alloc(context);
			if (obj == nullptr)
				return nullptr;
			obj->type = WObj::Type::Map;
			return obj;
		}

		static WObj* Func(WObj** argv, int argc, WContext* context) {
			// Not callable from user code
			WObj* obj = Alloc(context);
			if (obj == nullptr)
				return nullptr;
			obj->type = WObj::Type::Func;
			return obj;
		}

		static WObj* Object(WObj** argv, int argc, WContext* context) {
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

		static WObj* Userdata(WObj** argv, int argc, WContext* context) {
			// Not callable from user code
			WObj* obj = Alloc(context);
			if (obj == nullptr)
				return nullptr;
			obj->type = WObj::Type::Userdata;
			return obj;
		}

	}

	namespace attrlib {

		static WObj* Object_Pos(WObj** argv, int argc, WContext* context) {
			if (argc != 1) {
				SetInvalidArgumentCountError(context, "object.__pos__", argc, 1);
				return nullptr;
			}
			return argv[0];
		}

		static WObj* Object_Str(WObj** argv, int argc, WContext* context) {
			if (argc != 1) {
				SetInvalidArgumentCountError(context, "object.__str__", argc, 1);
				return nullptr;
			}

			std::string s = WObjToString(argv[0]);
			return WObjCreateString(context, s.c_str());
		}

		static WObj* Object_Eq(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "object.__eq__", argc, 2);
				return nullptr;
			}

			return WObjCreateBool(context, argv[0] == argv[1]);
		}

		static WObj* Null_Bool(WObj** argv, int argc, WContext* context) {
			if (argc != 1) {
				SetInvalidArgumentCountError(context, "NoneType.__nonzero__", argc, 1);
				return nullptr;
			} else if (!WObjIsNull(argv[0])) {
				SetInvalidTypeError(context, "NoneType.__nonzero__", 1, "NoneType", argv[0]);
				return nullptr;
			}

			return WObjCreateNull(context);
		}

		static WObj* Null_Eq(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "NoneType.__eq__", argc, 2);
				return nullptr;
			}

			return WObjCreateBool(context, WObjIsNull(argv[1]));
		}

		static WObj* Bool_Bool(WObj** argv, int argc, WContext* context) {
			if (argc != 1) {
				SetInvalidArgumentCountError(context, "bool.__nonzero__", argc, 1);
				return nullptr;
			} else if (!WObjIsBool(argv[0])) {
				SetInvalidTypeError(context, "bool.__nonzero__", 1, "bool", argv[0]);
				return nullptr;
			}

			return argv[0];
		}

		static WObj* Bool_Int(WObj** argv, int argc, WContext* context) {
			if (argc != 1) {
				SetInvalidArgumentCountError(context, "bool.__int__", argc, 1);
				return nullptr;
			} else if (!WObjIsBool(argv[0])) {
				SetInvalidTypeError(context, "bool.__int__", 1, "bool", argv[0]);
				return nullptr;
			}

			return WObjCreateInt(context, WObjGetBool(argv[0]) ? 1 : 0);
		}

		static WObj* Bool_Float(WObj** argv, int argc, WContext* context) {
			if (argc != 1) {
				SetInvalidArgumentCountError(context, "bool.__float__", argc, 1);
				return nullptr;
			} else if (!WObjIsBool(argv[0])) {
				SetInvalidTypeError(context, "bool.__float__", 1, "bool", argv[0]);
				return nullptr;
			}

			return WObjCreateFloat(context, WObjGetBool(argv[0]) ? (wfloat)1 : (wfloat)0);
		}

		static WObj* Bool_Eq(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "bool.__eq__", argc, 2);
				return nullptr;
			}

			return WObjCreateBool(context, WObjIsBool(argv[1]) && WObjGetBool(argv[0]) == WObjGetBool(argv[1]));
		}

		static WObj* Int_Bool(WObj** argv, int argc, WContext* context) {
			if (argc != 1) {
				SetInvalidArgumentCountError(context, "int.__nonzero__", argc, 1);
				return nullptr;
			} else if (!WObjIsInt(argv[0])) {
				SetInvalidTypeError(context, "int.__nonzero__", 1, "int", argv[0]);
				return nullptr;
			}

			return WObjCreateBool(context, WObjGetInt(argv[0]) != 0);
		}

		static WObj* Int_Int(WObj** argv, int argc, WContext* context) {
			if (argc != 1) {
				SetInvalidArgumentCountError(context, "int.__int__", argc, 1);
				return nullptr;
			} else if (!WObjIsInt(argv[0])) {
				SetInvalidTypeError(context, "int.__int__", 1, "int", argv[0]);
				return nullptr;
			}

			return argv[0];
		}

		static WObj* Int_Float(WObj** argv, int argc, WContext* context) {
			if (argc != 1) {
				SetInvalidArgumentCountError(context, "int.__float__", argc, 1);
				return nullptr;
			} else if (!WObjIsInt(argv[0])) {
				SetInvalidTypeError(context, "int.__float__", 1, "int", argv[0]);
				return nullptr;
			}

			return WObjCreateFloat(context, WObjGetFloat(argv[0]));
		}

		static WObj* Int_Eq(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "int.__eq__", argc, 2);
				return nullptr;
			}

			return WObjCreateBool(context, WObjIsInt(argv[1]) && WObjGetInt(argv[0]) == WObjGetInt(argv[1]));
		}

		static WObj* Int_Neg(WObj** argv, int argc, WContext* context) {
			if (argc != 1) {
				SetInvalidArgumentCountError(context, "int.__neg__", argc, 1);
				return nullptr;
			} else if (!WObjIsInt(argv[0])) {
				SetInvalidTypeError(context, "int.__neg__", 1, "int", argv[0]);
				return nullptr;
			}

			return WObjCreateInt(context, -WObjGetInt(argv[0]));
		}

		static WObj* Int_Add(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "int.__add__", argc, 1);
				return nullptr;
			} else if (!WObjIsInt(argv[0])) {
				SetInvalidTypeError(context, "int.__add__", 1, "int", argv[0]);
				return nullptr;
			}

			if (WObjIsInt(argv[1])) {
				return WObjCreateInt(context, WObjGetInt(argv[0]) + WObjGetInt(argv[1]));
			} else if (WObjIsIntOrFloat(argv[1])) {
				return WObjCreateFloat(context, WObjGetFloat(argv[0]) + WObjGetFloat(argv[1]));
			} else {
				SetInvalidTypeError(context, "int.__add__", 2, "int or float", argv[1]);
				return nullptr;
			}
		}

		static WObj* Int_Sub(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "int.__sub__", argc, 1);
				return nullptr;
			} else if (!WObjIsInt(argv[0])) {
				SetInvalidTypeError(context, "int.__sub__", 1, "int", argv[0]);
				return nullptr;
			}

			if (WObjIsInt(argv[1])) {
				return WObjCreateInt(context, WObjGetInt(argv[0]) - WObjGetInt(argv[1]));
			} else if (WObjIsIntOrFloat(argv[1])) {
				return WObjCreateFloat(context, WObjGetFloat(argv[0]) - WObjGetFloat(argv[1]));
			} else {
				SetInvalidTypeError(context, "int.__sub__", 2, "int or float", argv[1]);
				return nullptr;
			}
		}

		static WObj* Int_Mul(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "int.__mul__", argc, 1);
				return nullptr;
			} else if (!WObjIsInt(argv[0])) {
				SetInvalidTypeError(context, "int.__mul__", 1, "int", argv[0]);
				return nullptr;
			}

			if (WObjIsString(argv[1])) {
				wint multiplier = WObjGetInt(argv[0]);
				std::string s;
				for (wint i = 0; i < multiplier; i++)
					s += WObjGetString(argv[1]);
				return WObjCreateString(context, s.c_str());
			} else if (WObjIsInt(argv[1])) {
				return WObjCreateInt(context, WObjGetInt(argv[0]) * WObjGetInt(argv[1]));
			} else if (WObjIsIntOrFloat(argv[1])) {
				return WObjCreateFloat(context, WObjGetFloat(argv[0]) * WObjGetFloat(argv[1]));
			} else {
				SetInvalidTypeError(context, "int.__mul__", 2, "int or float", argv[1]);
				return nullptr;
			}
		}

		static WObj* Int_Div(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "int.__div__", argc, 1);
				return nullptr;
			} else if (!WObjIsInt(argv[0])) {
				SetInvalidTypeError(context, "int.__div__", 1, "int", argv[0]);
				return nullptr;
			}

			if (WObjIsIntOrFloat(argv[1])) {
				return WObjCreateFloat(context, WObjGetFloat(argv[0]) / WObjGetFloat(argv[1]));
			} else {
				SetInvalidTypeError(context, "int.__div__", 2, "int or float", argv[1]);
				return nullptr;
			}
		}

		static WObj* Int_FloorDiv(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "int.__floordiv__", argc, 1);
				return nullptr;
			} else if (!WObjIsInt(argv[0])) {
				SetInvalidTypeError(context, "int.__floordiv__", 1, "int", argv[0]);
				return nullptr;
			} else if (!WObjIsInt(argv[1])) {
				SetInvalidTypeError(context, "int.__floordiv__", 2, "int", argv[1]);
				return nullptr;
			}

			return WObjCreateInt(context, (wint)std::floor(WObjGetFloat(argv[0]) / WObjGetFloat(argv[1])));
		}

		static WObj* Int_Mod(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "int.__mod__", argc, 1);
				return nullptr;
			} else if (!WObjIsInt(argv[0])) {
				SetInvalidTypeError(context, "int.__mod__", 1, "int", argv[0]);
				return nullptr;
			} else if (!WObjIsIntOrFloat(argv[1])) {
				SetInvalidTypeError(context, "int.__mod__", 2, "int or float", argv[1]);
				return nullptr;
			}

			if (WObjIsInt(argv[1])) {
				wint mod = WObjGetInt(argv[1]);
				wint m = WObjGetInt(argv[0]) % mod;
				if (m < 0)
					m += mod;
				return WObjCreateInt(context, m);
			} else {
				return WObjCreateFloat(context, std::fmod(WObjGetFloat(argv[0]), WObjGetFloat(argv[1])));
			}
		}

		static WObj* Int_Pow(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "int.__pow__", argc, 1);
				return nullptr;
			} else if (!WObjIsInt(argv[0])) {
				SetInvalidTypeError(context, "int.__pow__", 1, "int", argv[0]);
				return nullptr;
			} else if (!WObjIsIntOrFloat(argv[1])) {
				SetInvalidTypeError(context, "int.__pow__", 2, "int or float", argv[1]);
				return nullptr;
			}

			return WObjCreateFloat(context, std::pow(WObjGetFloat(argv[0]), WObjGetFloat(argv[1])));
		}

		static WObj* Int_BitAnd(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "int.__and__", argc, 1);
				return nullptr;
			} else if (!WObjIsInt(argv[0])) {
				SetInvalidTypeError(context, "int.__and__", 1, "int", argv[0]);
				return nullptr;
			} else if (!WObjIsInt(argv[1])) {
				SetInvalidTypeError(context, "int.__and__", 2, "int", argv[1]);
				return nullptr;
			}

			return WObjCreateInt(context, WObjGetInt(argv[0]) & WObjGetInt(argv[1]));
		}

		static WObj* Int_BitOr(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "int.__or__", argc, 1);
				return nullptr;
			} else if (!WObjIsInt(argv[0])) {
				SetInvalidTypeError(context, "int.__or__", 1, "int", argv[0]);
				return nullptr;
			} else if (!WObjIsInt(argv[1])) {
				SetInvalidTypeError(context, "int.__or__", 2, "int", argv[1]);
				return nullptr;
			}

			return WObjCreateInt(context, WObjGetInt(argv[0]) | WObjGetInt(argv[1]));
		}

		static WObj* Int_BitXor(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "int.__xor__", argc, 1);
				return nullptr;
			} else if (!WObjIsInt(argv[0])) {
				SetInvalidTypeError(context, "int.__xor__", 1, "int", argv[0]);
				return nullptr;
			} else if (!WObjIsInt(argv[1])) {
				SetInvalidTypeError(context, "int.__xor__", 2, "int", argv[1]);
				return nullptr;
			}

			return WObjCreateInt(context, WObjGetInt(argv[0]) ^ WObjGetInt(argv[1]));
		}

		static WObj* Int_BitNot(WObj** argv, int argc, WContext* context) {
			if (argc != 1) {
				SetInvalidArgumentCountError(context, "int.__invert__", argc, 1);
				return nullptr;
			} else if (!WObjIsInt(argv[0])) {
				SetInvalidTypeError(context, "int.__invert__", 1, "int", argv[0]);
				return nullptr;
			}

			return WObjCreateInt(context, ~WObjGetInt(argv[0]));
		}

		static WObj* Int_ShiftLeft(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "int.__lshift__", argc, 1);
				return nullptr;
			} else if (!WObjIsInt(argv[0])) {
				SetInvalidTypeError(context, "int.__lshift__", 1, "int", argv[0]);
				return nullptr;
			} else if (!WObjIsInt(argv[1])) {
				SetInvalidTypeError(context, "int.__lshift__", 2, "int", argv[1]);
				return nullptr;
			}

			wint shift = WObjGetInt(argv[1]);
			if (shift < 0) {
				WErrorSetRuntimeError(context, "function int.__lshift__() shift was negative");
				return nullptr;
			}
			shift = std::min(shift, (wint)sizeof(wint) * 8);
			return WObjCreateInt(context, WObjGetInt(argv[0]) << shift);
		}

		static WObj* Int_ShiftRight(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "int.__rshift__", argc, 1);
				return nullptr;
			} else if (!WObjIsInt(argv[0])) {
				SetInvalidTypeError(context, "int.__rshift__", 1, "int", argv[0]);
				return nullptr;
			} else if (!WObjIsInt(argv[1])) {
				SetInvalidTypeError(context, "int.__rshift__", 2, "int", argv[1]);
				return nullptr;
			}

			wint shift = WObjGetInt(argv[1]);
			if (shift < 0) {
				WErrorSetRuntimeError(context, "function int.__rshift__() shift was negative");
				return nullptr;
			}
			shift = std::min(shift, (wint)sizeof(wint) * 8);
			return WObjCreateInt(context, WObjGetInt(argv[0]) >> shift);
		}

		static WObj* Float_Bool(WObj** argv, int argc, WContext* context) {
			if (argc != 1) {
				SetInvalidArgumentCountError(context, "float.__nonzero__", argc, 1);
				return nullptr;
			} else if (!WObjIsIntOrFloat(argv[0])) {
				SetInvalidTypeError(context, "float.__nonzero__", 1, "float", argv[0]);
				return nullptr;
			}

			return WObjCreateBool(context, WObjGetFloat(argv[0]) != 0);
		}

		static WObj* Float_Int(WObj** argv, int argc, WContext* context) {
			if (argc != 1) {
				SetInvalidArgumentCountError(context, "float.__int__", argc, 1);
				return nullptr;
			} else if (!WObjIsIntOrFloat(argv[0])) {
				SetInvalidTypeError(context, "float.__int__", 1, "float", argv[0]);
				return nullptr;
			}

			return WObjCreateInt(context, (wint)WObjGetFloat(argv[0]));
		}

		static WObj* Float_Float(WObj** argv, int argc, WContext* context) {
			if (argc != 1) {
				SetInvalidArgumentCountError(context, "float.__float__", argc, 1);
				return nullptr;
			} else if (!WObjIsIntOrFloat(argv[0])) {
				SetInvalidTypeError(context, "float.__float__", 1, "float", argv[0]);
				return nullptr;
			}

			return argv[0];
		}

		static WObj* Float_Eq(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "float.__eq__", argc, 2);
				return nullptr;
			}

			return WObjCreateBool(context, WObjIsIntOrFloat(argv[1]) && WObjGetFloat(argv[0]) == WObjGetFloat(argv[1]));
		}

		static WObj* Float_Neg(WObj** argv, int argc, WContext* context) {
			if (argc != 1) {
				SetInvalidArgumentCountError(context, "float.__neg__", argc, 1);
				return nullptr;
			} else if (!WObjIsIntOrFloat(argv[0])) {
				SetInvalidTypeError(context, "float.__neg__", 1, "float", argv[0]);
				return nullptr;
			}

			return WObjCreateFloat(context, -WObjGetFloat(argv[0]));
		}

		static WObj* Float_Add(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "float.__add__", argc, 1);
				return nullptr;
			} else if (!WObjIsIntOrFloat(argv[0])) {
				SetInvalidTypeError(context, "float.__add__", 1, "float", argv[0]);
				return nullptr;
			} else if (!WObjIsIntOrFloat(argv[1])) {
				SetInvalidTypeError(context, "float.__add__", 2, "float", argv[1]);
				return nullptr;
			}

			return WObjCreateFloat(context, WObjGetFloat(argv[0]) + WObjGetFloat(argv[1]));
		}

		static WObj* Float_Sub(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "float.__sub__", argc, 1);
				return nullptr;
			} else if (!WObjIsIntOrFloat(argv[0])) {
				SetInvalidTypeError(context, "float.__sub__", 1, "float", argv[0]);
				return nullptr;
			} else if (!WObjIsIntOrFloat(argv[1])) {
				SetInvalidTypeError(context, "float.__sub__", 2, "float", argv[1]);
				return nullptr;
			}

			return WObjCreateFloat(context, WObjGetFloat(argv[0]) - WObjGetFloat(argv[1]));
		}

		static WObj* Float_Mul(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "float.__mul__", argc, 1);
				return nullptr;
			} else if (!WObjIsIntOrFloat(argv[0])) {
				SetInvalidTypeError(context, "float.__mul__", 1, "float", argv[0]);
				return nullptr;
			} else if (!WObjIsIntOrFloat(argv[1])) {
				SetInvalidTypeError(context, "float.__mul__", 2, "float", argv[1]);
				return nullptr;
			}

			return WObjCreateFloat(context, WObjGetFloat(argv[0]) * WObjGetFloat(argv[1]));
		}

		static WObj* Float_Div(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "float.__div__", argc, 1);
				return nullptr;
			} else if (!WObjIsIntOrFloat(argv[0])) {
				SetInvalidTypeError(context, "float.__div__", 1, "float", argv[0]);
				return nullptr;
			} else if (!WObjIsIntOrFloat(argv[1])) {
				SetInvalidTypeError(context, "float.__div__", 2, "float", argv[1]);
				return nullptr;
			}

			return WObjCreateFloat(context, WObjGetFloat(argv[0]) / WObjGetFloat(argv[1]));
		}

		static WObj* Float_FloorDiv(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "float.__floordiv__", argc, 1);
				return nullptr;
			} else if (!WObjIsIntOrFloat(argv[0])) {
				SetInvalidTypeError(context, "float.__floordiv__", 1, "float", argv[0]);
				return nullptr;
			} else if (!WObjIsIntOrFloat(argv[1])) {
				SetInvalidTypeError(context, "float.__floordiv__", 2, "float", argv[1]);
				return nullptr;
			}

			return WObjCreateFloat(context, std::floor(WObjGetFloat(argv[0]) / WObjGetFloat(argv[1])));
		}

		static WObj* Float_Mod(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "int.__mod__", argc, 1);
				return nullptr;
			} else if (!WObjIsIntOrFloat(argv[0])) {
				SetInvalidTypeError(context, "float.__mod__", 1, "float", argv[0]);
				return nullptr;
			} else if (!WObjIsIntOrFloat(argv[1])) {
				SetInvalidTypeError(context, "float.__mod__", 2, "int or float", argv[1]);
				return nullptr;
			}

			return WObjCreateFloat(context, std::fmod(WObjGetFloat(argv[0]), WObjGetFloat(argv[1])));
		}

		static WObj* Float_Pow(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "int.__pow__", argc, 1);
				return nullptr;
			} else if (!WObjIsIntOrFloat(argv[0])) {
				SetInvalidTypeError(context, "float.__pow__", 1, "float", argv[0]);
				return nullptr;
			} else if (!WObjIsIntOrFloat(argv[1])) {
				SetInvalidTypeError(context, "float.__pow__", 2, "int or float", argv[1]);
				return nullptr;
			}

			return WObjCreateFloat(context, std::pow(WObjGetFloat(argv[0]), WObjGetFloat(argv[1])));
		}

		static WObj* Str_Bool(WObj** argv, int argc, WContext* context) {
			if (argc != 1) {
				SetInvalidArgumentCountError(context, "str.__nonzero__", argc, 1);
				return nullptr;
			} else if (!WObjIsString(argv[0])) {
				SetInvalidTypeError(context, "str.__nonzero__", 1, "str", argv[0]);
				return nullptr;
			}

			std::string s = WObjGetString(argv[0]);
			return WObjCreateBool(context, !s.empty());
		}

		static WObj* Str_Int(WObj** argv, int argc, WContext* context) {
			if (argc != 1) {
				SetInvalidArgumentCountError(context, "str.__int__", argc, 1);
				return nullptr;
			} else if (!WObjIsString(argv[0])) {
				SetInvalidTypeError(context, "str.__int__", 1, "str", argv[0]);
				return nullptr;
			}

			auto isDigit = [](char c, int base = 10) {
				switch (base) {
				case 2: return c >= '0' && c <= '1';
				case 8: return c >= '0' && c <= '7';
				case 10: return c >= '0' && c <= '9';
				case 16: return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
				default: WUNREACHABLE();
				}
			};

			auto digitValueOf = [](char c, int base) {
				switch (base) {
				case 2:
				case 8:
				case 10:
					return c - '0';
				case 16:
					if (c >= '0' && c <= '9') {
						return c - '0';
					} else if (c >= 'a' && c <= 'f') {
						return c - 'a' + 10;
					} else {
						return c - 'A' + 10;
					}
				default:
					WUNREACHABLE();
				}
			};

			std::string s = WObjGetString(argv[0]);
			const char* p = s.c_str();

			int base = 10;
			if (*p == '0') {
				switch (p[1]) {
				case 'b': case 'B': base = 2; break;
				case 'x': case 'X': base = 16; break;
				default: base = 8; break;
				}
			}

			if (base == 2 || base == 16) {
				p += 2;

				if (!isDigit(*p, base)) {
					if (base == 2) {
						WErrorSetRuntimeError(context, "function str.__int__() invalid binary string");
					} else {
						WErrorSetRuntimeError(context, "function str.__int__() invalid hexadecimal string");
					}
					return nullptr;
				}
			}

			uintmax_t value = 0;
			for (; *p && isDigit(*p, base); ++p) {
				value = (base * value) + digitValueOf(*p, base);
			}

			if (value > std::numeric_limits<wuint>::max()) {
				WErrorSetRuntimeError(context, "function str.__int__() Integer string is too large");
				return nullptr;
			}

			if (*p) {
				WErrorSetRuntimeError(context, "function str.__int__() invalid numerical string");
				return nullptr;
			}

			return WObjCreateInt(context, (wint)value);
		}

		static WObj* Str_Float(WObj** argv, int argc, WContext* context) {
			if (argc != 1) {
				SetInvalidArgumentCountError(context, "str.__float__", argc, 1);
				return nullptr;
			} else if (!WObjIsString(argv[0])) {
				SetInvalidTypeError(context, "str.__float__", 1, "str", argv[0]);
				return nullptr;
			}

			auto isDigit = [](char c, int base = 10) {
				switch (base) {
				case 2: return c >= '0' && c <= '1';
				case 8: return c >= '0' && c <= '7';
				case 10: return c >= '0' && c <= '9';
				case 16: return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
				default: WUNREACHABLE();
				}
			};

			auto digitValueOf = [](char c, int base) {
				switch (base) {
				case 2:
				case 8:
				case 10:
					return c - '0';
				case 16:
					if (c >= '0' && c <= '9') {
						return c - '0';
					} else if (c >= 'a' && c <= 'f') {
						return c - 'a' + 10;
					} else {
						return c - 'A' + 10;
					}
				default:
					WUNREACHABLE();
				}
			};

			std::string s = WObjGetString(argv[0]);
			const char* p = s.c_str();

			int base = 10;
			if (*p == '0') {
				switch (p[1]) {
				case 'b': case 'B': base = 2; break;
				case 'x': case 'X': base = 16; break;
				case '.': break;
				default: base = 8; break;
				}
			}

			if (base == 2 || base == 16) {
				p += 2;

				if (!isDigit(*p, base) && *p != '.') {
					if (base == 2) {
						WErrorSetRuntimeError(context, "function str.__float__() invalid binary string");
					} else {
						WErrorSetRuntimeError(context, "function str.__float__() invalid hexadecimal string");
					}
					return nullptr;
				}
			}

			uintmax_t value = 0;
			for (; *p && isDigit(*p, base); ++p) {
				value = (base * value) + digitValueOf(*p, base);
			}

			wfloat fvalue = (wfloat)value;
			if (*p == '.') {
				++p;
				for (int i = 1; *p && isDigit(*p, base); ++p, ++i) {
					fvalue += digitValueOf(*p, base) * std::pow((wfloat)base, (wfloat)-i);
				}
			}

			if (*p) {
				WErrorSetRuntimeError(context, "function str.__float__() invalid numerical string");
				return nullptr;
			}

			return WObjCreateFloat(context, fvalue);
		}

		static WObj* Str_Eq(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "str.__eq__", argc, 2);
				return nullptr;
			}

			return WObjCreateBool(context, WObjIsString(argv[1]) && std::strcmp(WObjGetString(argv[0]), WObjGetString(argv[1])) == 0);
		}

		static WObj* Str_Mul(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "str.__mul__", argc, 1);
				return nullptr;
			} else if (!WObjIsString(argv[0])) {
				SetInvalidTypeError(context, "str.__mul__", 1, "str", argv[0]);
				return nullptr;
			} else if (!WObjIsInt(argv[1])) {
				SetInvalidTypeError(context, "str.__mul__", 2, "int", argv[1]);
				return nullptr;
			}

			wint multiplier = WObjGetInt(argv[1]);
			std::string s;
			for (wint i = 0; i < multiplier; i++)
				s += WObjGetString(argv[0]);
			return WObjCreateString(context, s.c_str());
		}

		static WObj* Str_Contains(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "str.__contains__", argc, 1);
				return nullptr;
			} else if (!WObjIsString(argv[0])) {
				SetInvalidTypeError(context, "str.__contains__", 1, "str", argv[0]);
				return nullptr;
			} else if (!WObjIsString(argv[1])) {
				SetInvalidTypeError(context, "str.__contains__", 2, "str", argv[1]);
				return nullptr;
			}

			return WObjCreateBool(context, std::strstr(WObjGetString(argv[0]), WObjGetString(argv[1])));
		}

		static WObj* List_GetItem(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "list.__getitem__", argc, 2);
				return nullptr;
			} else if (!WObjIsList(argv[0])) {
				SetInvalidTypeError(context, "list.__getitem__", 1, "list", argv[0]);
				return nullptr;
			} else if (!WObjIsInt(argv[1])) {
				SetInvalidTypeError(context, "list.__getitem__", 2, "int", argv[1]);
				return nullptr;
			}

			size_t rawIndex = ConvertNegativeIndex(WObjGetInt(argv[1]), argv[0]->v.size());
			if (rawIndex >= argv[0]->v.size()) {
				SetIndexOutOfRangeError(context, "list.__getitem__", 2);
				return nullptr;
			}

			return argv[0]->v[rawIndex];
		}

		static WObj* List_SetItem(WObj** argv, int argc, WContext* context) {
			if (argc != 3) {
				SetInvalidArgumentCountError(context, "list.__setitem__", argc, 3);
				return nullptr;
			} else if (!WObjIsList(argv[0])) {
				SetInvalidTypeError(context, "list.__setitem__", 1, "list", argv[0]);
				return nullptr;
			} else if (!WObjIsInt(argv[1])) {
				SetInvalidTypeError(context, "list.__setitem__", 2, "int", argv[1]);
				return nullptr;
			}

			size_t rawIndex = ConvertNegativeIndex(WObjGetInt(argv[1]), argv[0]->v.size());
			if (rawIndex >= argv[0]->v.size()) {
				SetIndexOutOfRangeError(context, "list.__setitem__", 2);
				return nullptr;
			}

			argv[0]->v[rawIndex] = argv[2];
			return argv[0];
		}

		static WObj* List_Append(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "list.append", argc, 2);
				return nullptr;
			} else if (!WObjIsList(argv[0])) {
				SetInvalidTypeError(context, "list.append", 1, "list", argv[0]);
				return nullptr;
			}

			argv[0]->v.push_back(argv[1]);
			return argv[0];
		}

		static WObj* List_Insert(WObj** argv, int argc, WContext* context) {
			if (argc != 3) {
				SetInvalidArgumentCountError(context, "list.insert", argc, 3);
				return nullptr;
			} else if (!WObjIsList(argv[0])) {
				SetInvalidTypeError(context, "list.insert", 1, "list", argv[0]);
				return nullptr;
			} else if (!WObjIsInt(argv[1])) {
				SetInvalidTypeError(context, "list.insert", 2, "int", argv[1]);
				return nullptr;
			}

			wint index = WObjGetInt(argv[1]);
			size_t rawIndex = ConvertNegativeIndex(index, argv[0]->v.size());
			if (rawIndex >= argv[0]->v.size()) {
				SetIndexOutOfRangeError(context, "list.insert", 2);
				return nullptr;
			}

			argv[0]->v.insert(argv[0]->v.begin() + rawIndex, argv[2]);
			return argv[0];
		}

		static WObj* List_Pop(WObj** argv, int argc, WContext* context) {
			if (argc >= 2) {
				SetInvalidArgumentCountError(context, "list.pop", argc, 2);
				return nullptr;
			} else if (!WObjIsList(argv[0])) {
				SetInvalidTypeError(context, "list.pop", 1, "list", argv[0]);
				return nullptr;
			}

			int index = -1;
			if (argc == 1) {
				if (!WObjIsInt(argv[1])) {
					SetInvalidTypeError(context, "list.pop", 2, "int", argv[1]);
					return nullptr;
				} else {
					index = WObjGetInt(argv[1]);
				}
			}

			size_t rawIndex = ConvertNegativeIndex(index, argv[0]->v.size());
			if (rawIndex >= argv[0]->v.size()) {
				SetIndexOutOfRangeError(context, "list.pop", 2);
				return nullptr;
			}

			WObj* popped = argv[0]->v[rawIndex];
			argv[0]->v.erase(argv[0]->v.begin() + rawIndex);
			return popped;
		}

		static WObj* List_Remove(WObj** argv, int argc, WContext* context) {
			if (argc != 2) {
				SetInvalidArgumentCountError(context, "list.remove", argc, 2);
				return nullptr;
			} else if (!WObjIsList(argv[0])) {
				SetInvalidTypeError(context, "list.remove", 1, "list", argv[0]);
				return nullptr;
			}

			auto& v = argv[0]->v;
			for (size_t i = 0; i < v.size(); i++) {
				WObj* eq = WOpEquals(argv[1], v[i]);

				if (eq == nullptr) {
					return nullptr;
				}
				
				if (!WObjGetBool(eq)) {
					continue;
				}

				if (i >= v.size()) {
					WErrorSetRuntimeError(context, "function list.remove() list modified while iterating over it");
					return nullptr;
				}
				
				v.erase(v.begin() + i);
				return argv[0];
			}

			WErrorSetRuntimeError(context, "function list.remove() ");
			return nullptr;
		}

	} // namespace attrlib

	namespace lib {

		static WObj* print(WObj** argv, int argc, WContext* context) {
			std::string text;
			for (int i = 0; i < argc; i++) {
				if (WObj* s = WOpCastToString(argv[i])) {
					text += WObjGetString(s);
				} else {
					return nullptr;
				}

				if (i < argc - 1) {
					text += ' ';
				}
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
		CheckOperation(context->builtinClasses.null = CreateClass<classlib::Null>(context));
		CheckOperation(context->builtinClasses._bool = CreateClass<classlib::Bool>(context, "bool"));
		CheckOperation(context->builtinClasses._int = CreateClass<classlib::Int>(context, "int"));
		CheckOperation(context->builtinClasses._float = CreateClass<classlib::Float>(context, "float"));
		CheckOperation(context->builtinClasses.str = CreateClass<classlib::Str>(context, "str"));
		CheckOperation(context->builtinClasses.list = CreateClass<classlib::List>(context, "list"));
		CheckOperation(context->builtinClasses.map = CreateClass<classlib::Map>(context, "dict"));
		CheckOperation(context->builtinClasses.func = CreateClass<classlib::Func>(context));
		CheckOperation(context->builtinClasses.object = CreateClass<classlib::Object>(context, "object"));
		CheckOperation(context->builtinClasses.userdata = CreateClass<classlib::Userdata>(context));

		// Subclass the object class
		AttributeTable& objectAttributes = context->builtinClasses.object->c;
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
		CheckOperation(context->nullSingleton = WOpCall(context->builtinClasses.null, nullptr, 0));

		// Register methods of builtin classes
		CheckOperation(RegisterStatelessMethod<attrlib::Object_Pos>(context, context->builtinClasses.object->c, "__pos__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Object_Str>(context, context->builtinClasses.object->c, "__str__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Object_Eq>(context, context->builtinClasses.object->c, "__eq__"));

		CheckOperation(RegisterStatelessMethod<attrlib::Null_Bool>(context, context->builtinClasses.null->c, "__bool__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Null_Eq>(context, context->builtinClasses.null->c, "__eq__"));

		CheckOperation(RegisterStatelessMethod<attrlib::Bool_Bool>(context, context->builtinClasses._bool->c, "__bool__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Bool_Int>(context, context->builtinClasses._bool->c, "__int__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Bool_Float>(context, context->builtinClasses._bool->c, "__float__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Bool_Eq>(context, context->builtinClasses._bool->c, "__eq__"));

		CheckOperation(RegisterStatelessMethod<attrlib::Int_Bool>(context, context->builtinClasses._int->c, "__bool__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Int_Int>(context, context->builtinClasses._int->c, "__int__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Int_Float>(context, context->builtinClasses._int->c, "__float__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Int_Eq>(context, context->builtinClasses._int->c, "__eq__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Int_Neg>(context, context->builtinClasses._int->c, "__neg__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Int_Add>(context, context->builtinClasses._int->c, "__add__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Int_Sub>(context, context->builtinClasses._int->c, "__sub__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Int_Mul>(context, context->builtinClasses._int->c, "__mul__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Int_Div>(context, context->builtinClasses._int->c, "__div__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Int_FloorDiv>(context, context->builtinClasses._int->c, "__floordiv__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Int_Mod>(context, context->builtinClasses._int->c, "__mod__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Int_Pow>(context, context->builtinClasses._int->c, "__pow__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Int_BitAnd>(context, context->builtinClasses._int->c, "__and__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Int_BitOr>(context, context->builtinClasses._int->c, "__or__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Int_BitXor>(context, context->builtinClasses._int->c, "__xor__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Int_BitNot>(context, context->builtinClasses._int->c, "__invert__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Int_ShiftLeft>(context, context->builtinClasses._int->c, "__lshift__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Int_ShiftRight>(context, context->builtinClasses._int->c, "__rshift__"));

		CheckOperation(RegisterStatelessMethod<attrlib::Float_Bool>(context, context->builtinClasses._float->c, "__bool__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Float_Int>(context, context->builtinClasses._float->c, "__float__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Float_Float>(context, context->builtinClasses._float->c, "__float__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Float_Eq>(context, context->builtinClasses._float->c, "__eq__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Float_Neg>(context, context->builtinClasses._float->c, "__neg__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Float_Add>(context, context->builtinClasses._float->c, "__add__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Float_Sub>(context, context->builtinClasses._float->c, "__sub__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Float_Mul>(context, context->builtinClasses._float->c, "__mul__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Float_Div>(context, context->builtinClasses._float->c, "__div__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Float_FloorDiv>(context, context->builtinClasses._float->c, "__floordiv__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Float_Mod>(context, context->builtinClasses._float->c, "__mod__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Float_Pow>(context, context->builtinClasses._float->c, "__pow__"));

		CheckOperation(RegisterStatelessMethod<attrlib::Str_Bool>(context, context->builtinClasses.str->c, "__nonzero__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Str_Int>(context, context->builtinClasses.str->c, "__int__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Str_Float>(context, context->builtinClasses.str->c, "__float__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Str_Eq>(context, context->builtinClasses.str->c, "__eq__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Str_Mul>(context, context->builtinClasses.str->c, "__mul__"));
		CheckOperation(RegisterStatelessMethod<attrlib::Str_Contains>(context, context->builtinClasses.str->c, "__contains__"));

		CheckOperation(RegisterStatelessMethod<attrlib::List_GetItem>(context, context->builtinClasses.list->c, "__getitem__"));
		CheckOperation(RegisterStatelessMethod<attrlib::List_SetItem>(context, context->builtinClasses.list->c, "__setitem__"));
		//CheckOperation(RegisterStatelessMethod<attrlib::List_Contains>(context, context->builtinClasses.list->c, "__contains__"));
		CheckOperation(RegisterStatelessMethod<attrlib::List_Insert>(context, context->builtinClasses.list->c, "insert"));
		CheckOperation(RegisterStatelessMethod<attrlib::List_Append>(context, context->builtinClasses.list->c, "append"));
		//CheckOperation(RegisterStatelessMethod<attrlib::List_Extend>(context, context->builtinClasses.list->c, "extend"));
		CheckOperation(RegisterStatelessMethod<attrlib::List_Pop>(context, context->builtinClasses.list->c, "pop"));
		CheckOperation(RegisterStatelessMethod<attrlib::List_Remove>(context, context->builtinClasses.list->c, "remove"));
		//CheckOperation(RegisterStatelessMethod<attrlib::List_Clear>(context, context->builtinClasses.list->c, "clear"));
		//CheckOperation(RegisterStatelessMethod<attrlib::List_Copy>(context, context->builtinClasses.list->c, "copy"));
		//CheckOperation(RegisterStatelessMethod<attrlib::List_Index>(context, context->builtinClasses.list->c, "index"));
		//CheckOperation(RegisterStatelessMethod<attrlib::List_Reverse>(context, context->builtinClasses.list->c, "reverse"));
		//CheckOperation(RegisterStatelessMethod<attrlib::List_Count>(context, context->builtinClasses.list->c, "count"));


		// Register builtin functions
		CheckOperation(RegisterStatelessFunction<lib::print>(context, "print"));
		//CheckOperation(RegisterStatelessFunction<lib::range>(context, "range"));

		return true;
	}

} // namespace wings
