#include "impl.h"
#include "gc.h"
#include <iostream>
#include <sstream>
#include <unordered_set>
#include <cmath>
#include <algorithm>
#include <queue>

using namespace wings;

static const char* const BOOTSTRAP_CODE = R"(
class bool:
	def __str__(self):
		return "True" if self else "False"

class int:
	pass

class float:
	pass

class str:
	def __str__(self):
		return self

class list:
	pass

class dict:
	pass
)";

static const char* const LIBRARY_CODE = R"(
def __str__(self):
	return "None"
set_method(None, "__str__", __str__)

def __nonzero__(self):
	return False
set_method(None, "__nonzero__", __nonzero__)

def append(self, x):
	self.__insertindex(len(self), x)
set_method(list, "append", append)

class BaseException:
	def __init__(self, message=""):
		self.message = message
	def __str__(self):
		return self.message

class Exception(BaseException):
	pass

class SyntaxError(Exception):
	pass

class NameError(Exception):
	pass

class TypeError(Exception):
	pass

class ValueError(Exception):
	pass

class AttributeError(Exception):
	pass

class LookupError(Exception):
	pass

class IndexError(LookupError):
	pass

class KeyError(LookupError):
	pass

class ArithmeticError(Exception):
	pass

class OverflowError(ArithmeticError):
	pass

class ZeroDivisionError(ArithmeticError):
	pass

def len(x):
	return x.__len__()

def repr(x):
	return x.__repr__()

def next(x):
	return x.__next__()

class __Range:
	def __init__(self, start, end, step):
		self.start = start
		self.end = end
		self.step = step
	def __iter__(self):
		return __RangeIter(self.start, self.end, self.step)

class __RangeIter:
	def __init__(self, start, end, step):
		self.cur = start
		self.end = end
		self.step = step
	def __next__(self):
		cur = self.cur
		self.cur = self.cur + self.step
		return cur
	def __end__(self):
		if self.step > 0:
			return self.cur >= self.end
		else:
			return self.cur <= self.end

def range(start, end=None, step=None):
	if end == None:
		return __Range(0, start, 1)
	elif step == None:
		return __Range(start, end, 1)
	else:
		return __Range(start, end, step)

def __convert_index(container, index):
	if index < 0:
		return len(container) + index
	else:
		return index

def __convert_slice(container, index):
	index.start = __convert_index(container, index.start) if index.start != None else 0
	index.stop = __convert_index(container, index.stop)
	index.step = index.step if index.start != None else 1

class slice:
	def __init__(self, start, stop=None, step=None):
		if stop == None:
			self.start = None
			self.stop = start
			self.step = None
		elif step == None:
			self.start = start
			self.stop = stop
			self.step = None
		else:
			self.start = start
			self.stop = stop
			self.step = step

class __ListIter:
	def __init__(self, li):
		self.li = li
		self.i = 0

	def __next__(self):
		v = self.li[self.i]
		self.i = self.i + 1
		return v

	def __end__(self):
		return self.i >= len(self.li)
		
def __getitem__(self, index):
	if isinstance(index, slice):
		index = __convert_slice(index)
		return self.__getslice(
			index.start,
			index.stop,
			index.step
		)
	else:
		return self.__getindex(__convert_index(self, index))
		
def __setitem__(self, index, value):
	if isinstance(index, slice):
		index = __convert_slice(index)
		return self.__setslice(
			index.start,
			index.stop,
			index.step,
			value
		)
	else:
		return self.__setindex(__convert_index(self, index), value)

set_method(tuple, "__iter__", lambda self: __ListIter(self))
set_method(list, "__iter__", lambda self: __ListIter(self))
set_method(tuple, "__getitem__", __getitem__)
set_method(list, "__getitem__", __getitem__)
set_method(tuple, "__setitem__", __setitem__)
set_method(list, "__setitem__", __setitem__)

set_method = None
__getitem__ = None
__setitem__ = None

# Cause these values to be cached
True
False
)";


enum class Collection {
	List,
	Tuple,
};

static std::string PtrToString(const void* p) {
	std::stringstream ss;
	ss << p;
	return ss.str();
}

#define EXPECT_ARG_COUNT(n) do if (argc != n) { WRaiseArgumentCountError(context, argc, n); return nullptr; } while (0)
#define EXPECT_ARG_COUNT_BETWEEN(min, max) do if (argc < min || argc > max) { WRaiseArgumentCountError(context, argc, -1); return nullptr; } while (0)
#define EXPECT_ARG_TYPE(index, check, expect) do if (!(check)(argv[index])) { WRaiseArgumentTypeError(context, index, expect); return nullptr; } while (0)
#define EXPECT_ARG_TYPE_NULL(index) EXPECT_ARG_TYPE(index, WIsNone, "NoneType");
#define EXPECT_ARG_TYPE_BOOL(index) EXPECT_ARG_TYPE(index, WIsBool, "bool");
#define EXPECT_ARG_TYPE_INT(index) EXPECT_ARG_TYPE(index, WIsInt, "int");
#define EXPECT_ARG_TYPE_FLOAT(index) EXPECT_ARG_TYPE(index, [](const WObj* v) { return WIsIntOrFloat(v) && !WIsInt(v); }, "int or float");
#define EXPECT_ARG_TYPE_INT_OR_FLOAT(index) EXPECT_ARG_TYPE(index, WIsIntOrFloat, "int or float");
#define EXPECT_ARG_TYPE_STRING(index) EXPECT_ARG_TYPE(index, WIsString, "str");
#define EXPECT_ARG_TYPE_LIST(index) EXPECT_ARG_TYPE(index, WIsList, "list");
#define EXPECT_ARG_TYPE_TUPLE(index) EXPECT_ARG_TYPE(index, WIsTuple, "tuple");
#define EXPECT_ARG_TYPE_MAP(index) EXPECT_ARG_TYPE(index, WIsDictionary, "dict");
#define EXPECT_ARG_TYPE_FUNC(index) EXPECT_ARG_TYPE(index, WIsFunction, "function");

namespace wings {

	namespace ctors {

		static WObj* _bool(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT_BETWEEN(1, 2);

			bool v{};
			if (argc == 1) {
				v = false;
			} else {
				WObj* res = WConvertToBool(argv[1]);
				if (res == nullptr)
					return nullptr;
				v = WGetBool(res);
			}

			argv[0]->attributes = context->builtins._bool->Get<WObj::Class>().instanceAttributes.Copy();
			argv[0]->type = "__bool";
			argv[0]->data = new bool(v);
			argv[0]->finalizer.fptr = [](WObj* obj, void*) { delete (bool*)obj->data; };

			return WCreateNone(context);
		}

		static WObj* _int(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT_BETWEEN(1, 2);

			wint v{};
			if (argc == 1) {
				v = 0;
			} else {
				WObj* res = WConvertToInt(argv[1]);
				if (res == nullptr)
					return nullptr;
				v = WGetInt(res);
			}

			argv[0]->attributes = context->builtins._int->Get<WObj::Class>().instanceAttributes.Copy();
			argv[0]->type = "__int";
			argv[0]->data = new wint(v);
			argv[0]->finalizer.fptr = [](WObj* obj, void*) { delete (wint*)obj->data; };

			return WCreateNone(context);
		}

		static WObj* _float(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT_BETWEEN(1, 2);

			wfloat v{};
			if (argc == 1) {
				v = 0;
			} else {
				WObj* res = WConvertToFloat(argv[1]);
				if (res == nullptr)
					return nullptr;
				v = WGetFloat(res);
			}

			argv[0]->attributes = context->builtins._float->Get<WObj::Class>().instanceAttributes.Copy();
			argv[0]->type = "__float";
			argv[0]->data = new wfloat(v);
			argv[0]->finalizer.fptr = [](WObj* obj, void*) { delete (wfloat*)obj->data; };

			return WCreateNone(context);
		}

		static WObj* str(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT_BETWEEN(1, 2);

			std::string v{};
			if (argc == 2) {
				WObj* res = WConvertToString(argv[1]);
				if (res == nullptr)
					return nullptr;
				v = WGetString(res);
			}

			argv[0]->attributes = context->builtins.str->Get<WObj::Class>().instanceAttributes.Copy();
			argv[0]->type = "__str";
			argv[0]->data = new std::string(std::move(v));
			argv[0]->finalizer.fptr = [](WObj* obj, void*) { delete (std::string*)obj->data; };

			return WCreateNone(context);
		}

		template <Collection collection_t>
		static WObj* collection(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT_BETWEEN(1, 2);

			struct State {
				std::vector<WObj*> v;
				std::vector<WObjRef> refs;
			} s;
			if (argc == 2) {
				auto f = [](WObj* x, void* u) {
					State* s = (State*)u;
					s->refs.emplace_back(x);
					s->v.push_back(x);
					return true;
				};

				if (!WIterate(argv[1], &s, f))
					return nullptr;
			}

			if constexpr (collection_t == Collection::List) {
				argv[0]->attributes = context->builtins.list->Get<WObj::Class>().instanceAttributes.Copy();
				argv[0]->type = "__list";
			} else {
				argv[0]->attributes = context->builtins.tuple->Get<WObj::Class>().instanceAttributes.Copy();
				argv[0]->type = "__tuple";
			}
			argv[0]->data = new std::vector<WObj*>(std::move(s.v));
			argv[0]->finalizer.fptr = [](WObj* obj, void*) { delete (std::vector<WObj*>*)obj->data; };

			return WCreateNone(context);
		}

		static WObj* map(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT_BETWEEN(1, 2);

			argv[0]->attributes = context->builtins.dict->Get<WObj::Class>().instanceAttributes.Copy();
			argv[0]->type = "__map";
			argv[0]->data = new wings::WDict();
			argv[0]->finalizer.fptr = [](WObj* obj, void*) { delete (wings::WDict*)obj->data; };

			return WCreateNone(context);
		}

		static WObj* func(WObj** argv, int argc, WObj* kwargs, void* ud) {
			// Not callable from user code

			//argv[0]->attributes = context->builtins.func->Get<WObj::Class>().instanceAttributes.Copy();
			argv[0]->type = "__func";
			argv[0]->data = new WObj::Func();
			argv[0]->finalizer.fptr = [](WObj* obj, void*) { delete (WObj::Func*)obj->data; };

			return nullptr;
			//return WCreateNone(context);
		}
	} // namespace ctors

	namespace attrlib {

		static WObj* object_pos(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			return argv[0];
		}

		static WObj* object_str(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			std::string s = "<" + WObjTypeToString(argv[0]) + " object at " + PtrToString(argv[0]) + ">";
			return WCreateString(context, s.c_str());
		}

		static WObj* object_eq(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			return WCreateBool(context, argv[0] == argv[1]);
		}

		static WObj* object_ne(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			if (WObj* eq = WEquals(argv[0], argv[1])) {
				return WCreateBool(context, !WGetBool(eq));
			} else {
				return nullptr;
			}
		}

		static WObj* object_iadd(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			return WCallMethod(argv[0], "__add__", &argv[1], 1);
		}

		static WObj* null_nonzero(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_NULL(0);
			return WCreateNone(context);
		}

		static WObj* null_eq(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_NULL(0);
			return WCreateBool(context, WIsNone(argv[1]));
		}

		static WObj* bool_nonzero(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_BOOL(0);
			return argv[0];
		}

		static WObj* bool_int(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_BOOL(0);
			return WCreateInt(context, WGetBool(argv[0]) ? 1 : 0);
		}

		static WObj* bool_float(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_BOOL(0);
			return WCreateFloat(context, WGetBool(argv[0]) ? (wfloat)1 : (wfloat)0);
		}

		static WObj* bool_eq(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_BOOL(0);
			return WCreateBool(context, WIsBool(argv[1]) && WGetBool(argv[0]) == WGetBool(argv[1]));
		}

		static WObj* int_nonzero(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_INT(0);
			return WCreateBool(context, WGetInt(argv[0]) != 0);
		}

		static WObj* int_int(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_INT(0);
			return argv[0];
		}

		static WObj* int_float(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_INT(0);
			return WCreateFloat(context, WGetFloat(argv[0]));
		}

		static WObj* int_str(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_INT(0);
			return WCreateString(context, std::to_string(argv[0]->Get<wint>()).c_str());
		}

		static WObj* int_eq(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);
			return WCreateBool(context, WIsInt(argv[1]) && WGetInt(argv[0]) == WGetInt(argv[1]));
		}

		static WObj* int_gt(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return WCreateBool(context, WGetFloat(argv[0]) > WGetFloat(argv[1]));
		}

		static WObj* int_ge(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return WCreateBool(context, WGetFloat(argv[0]) >= WGetFloat(argv[1]));
		}

		static WObj* int_lt(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return WCreateBool(context, WGetFloat(argv[0]) < WGetFloat(argv[1]));
		}

		static WObj* int_le(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return WCreateBool(context, WGetFloat(argv[0]) <= WGetFloat(argv[1]));
		}

		static WObj* int_neg(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_INT(0);
			return WCreateInt(context, -WGetInt(argv[0]));
		}

		static WObj* int_add(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			if (WIsInt(argv[1])) {
				return WCreateInt(context, WGetInt(argv[0]) + WGetInt(argv[1]));
			} else {
				return WCreateFloat(context, WGetFloat(argv[0]) + WGetFloat(argv[1]));
			}
		}

		static WObj* int_sub(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			if (WIsInt(argv[1])) {
				return WCreateInt(context, WGetInt(argv[0]) - WGetInt(argv[1]));
			} else {
				return WCreateFloat(context, WGetFloat(argv[0]) - WGetFloat(argv[1]));
			}
		}

		static WObj* int_mul(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);

			if (WIsString(argv[1])) {
				wint multiplier = WGetInt(argv[0]);
				std::string s;
				for (wint i = 0; i < multiplier; i++)
					s += WGetString(argv[1]);
				return WCreateString(context, s.c_str());
			} else if (WIsInt(argv[1])) {
				return WCreateInt(context, WGetInt(argv[0]) * WGetInt(argv[1]));
			} else if (WIsIntOrFloat(argv[1])) {
				return WCreateFloat(context, WGetFloat(argv[0]) * WGetFloat(argv[1]));
			} else {
				EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
				return nullptr;
			}
		}

		static WObj* int_truediv(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(0);

			if (WGetFloat(argv[1]) == 0) {
				WRaiseZeroDivisionError(context);
				return nullptr;
			}
			return WCreateFloat(context, WGetFloat(argv[0]) / WGetFloat(argv[1]));
		}

		static WObj* int_floordiv(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);

			if (WGetFloat(argv[1]) == 0) {
				WRaiseZeroDivisionError(context);
				return nullptr;
			}

			if (WIsInt(argv[1])) {
				return WCreateInt(context, (wint)std::floor(WGetFloat(argv[0]) / WGetFloat(argv[1])));
			} else {
				return WCreateFloat(context, std::floor(WGetFloat(argv[0]) / WGetFloat(argv[1])));
			}
		}

		static WObj* int_mod(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);

			if (WGetFloat(argv[1]) == 0) {
				WRaiseZeroDivisionError(context);
				return nullptr;
			}

			if (WIsInt(argv[1])) {
				wint mod = WGetInt(argv[1]);
				wint m = WGetInt(argv[0]) % mod;
				if (m < 0)
					m += mod;
				return WCreateInt(context, m);
			} else {
				return WCreateFloat(context, std::fmod(WGetFloat(argv[0]), WGetFloat(argv[1])));
			}
		}

		static WObj* int_pow(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return WCreateFloat(context, std::pow(WGetFloat(argv[0]), WGetFloat(argv[1])));
		}

		static WObj* int_and(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);
			EXPECT_ARG_TYPE_INT(1);
			return WCreateInt(context, WGetInt(argv[0]) & WGetInt(argv[1]));
		}

		static WObj* int_or(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);
			EXPECT_ARG_TYPE_INT(1);
			return WCreateInt(context, WGetInt(argv[0]) | WGetInt(argv[1]));
		}

		static WObj* int_xor(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);
			EXPECT_ARG_TYPE_INT(1);
			return WCreateInt(context, WGetInt(argv[0]) ^ WGetInt(argv[1]));
		}

		static WObj* int_invert(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_INT(0);
			return WCreateInt(context, ~WGetInt(argv[0]));
		}

		static WObj* int_lshift(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);
			EXPECT_ARG_TYPE_INT(1);

			wint shift = WGetInt(argv[1]);
			if (shift < 0) {
				WRaiseException(context, "Shift cannot be negative", context->builtins.valueError);
				return nullptr;
			}
			shift = std::min(shift, (wint)sizeof(wint) * 8);
			return WCreateInt(context, WGetInt(argv[0]) << shift);
		}

		static WObj* int_rshift(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);
			EXPECT_ARG_TYPE_INT(1);

			wint shift = WGetInt(argv[1]);
			if (shift < 0) {
				WRaiseException(context, "Shift cannot be negative", context->builtins.valueError);
				return nullptr;
			}
			shift = std::min(shift, (wint)sizeof(wint) * 8);
			return WCreateInt(context, WGetInt(argv[0]) >> shift);
		}

		static WObj* float_nonzero(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			return WCreateBool(context, WGetFloat(argv[0]) != 0);
		}

		static WObj* float_int(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			return WCreateInt(context, (wint)WGetFloat(argv[0]));
		}

		static WObj* float_float(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			return argv[0];
		}

		static WObj* float_str(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_FLOAT(0);
			std::string s = std::to_string(argv[0]->Get<wfloat>());
			s.erase(s.find_last_not_of('0') + 1, std::string::npos);
			if (s.ends_with('.'))
				s.push_back('0');
			return WCreateString(context, s.c_str());
		}

		static WObj* float_eq(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			return WCreateBool(context, WIsIntOrFloat(argv[1]) && WGetFloat(argv[0]) == WGetFloat(argv[1]));
		}

		static WObj* float_neg(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			return WCreateFloat(context, -WGetFloat(argv[0]));
		}

		static WObj* float_add(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return WCreateFloat(context, WGetFloat(argv[0]) + WGetFloat(argv[1]));
		}

		static WObj* float_sub(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return WCreateFloat(context, WGetFloat(argv[0]) - WGetFloat(argv[1]));
		}

		static WObj* float_mul(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return WCreateFloat(context, WGetFloat(argv[0]) * WGetFloat(argv[1]));
		}

		static WObj* float_truediv(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return WCreateFloat(context, WGetFloat(argv[0]) / WGetFloat(argv[1]));
		}

		static WObj* float_floordiv(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return WCreateFloat(context, std::floor(WGetFloat(argv[0]) / WGetFloat(argv[1])));
		}

		static WObj* float_mod(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return WCreateFloat(context, std::fmod(WGetFloat(argv[0]), WGetFloat(argv[1])));
		}

		static WObj* float_pow(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return WCreateFloat(context, std::pow(WGetFloat(argv[0]), WGetFloat(argv[1])));
		}

		static WObj* str_nonzero(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_STRING(0);
			std::string s = WGetString(argv[0]);
			return WCreateBool(context, !s.empty());
		}

		static WObj* str_int(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_STRING(0);

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

			std::string s = WGetString(argv[0]);
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
						WRaiseException(context, "Invalid binary string", context->builtins.valueError);
					} else {
						WRaiseException(context, "Invalid hexadecimal string", context->builtins.valueError);
					}
					return nullptr;
				}
			}

			uintmax_t value = 0;
			for (; *p && isDigit(*p, base); ++p) {
				value = (base * value) + digitValueOf(*p, base);
			}

			if (value > std::numeric_limits<wuint>::max()) {
				WRaiseException(context, "Integer string is too large", context->builtins.overflowError);
				return nullptr;
			}

			if (*p) {
				WRaiseException(context, "Invalid integer string", context->builtins.valueError);
				return nullptr;
			}

			return WCreateInt(context, (wint)value);
		}

		static WObj* str_float(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_STRING(0);

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

			std::string s = WGetString(argv[0]);
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
						WRaiseException(context, "Invalid binary string", context->builtins.valueError);
					} else {
						WRaiseException(context, "Invalid hexadecimal string", context->builtins.valueError);
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
				WRaiseException(context, "Invalid float string", context->builtins.valueError);
				return nullptr;
			}

			return WCreateFloat(context, fvalue);
		}

		static WObj* str_eq(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_STRING(0);
			return WCreateBool(context, WIsString(argv[1]) && std::strcmp(WGetString(argv[0]), WGetString(argv[1])) == 0);
		}

		static WObj* str_mul(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_STRING(0);
			EXPECT_ARG_TYPE_INT(1);
			wint multiplier = WGetInt(argv[1]);
			std::string s;
			for (wint i = 0; i < multiplier; i++)
				s += WGetString(argv[0]);
			return WCreateString(context, s.c_str());
		}

		static WObj* str_contains(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_STRING(0);
			EXPECT_ARG_TYPE_STRING(1);
			return WCreateBool(context, std::strstr(WGetString(argv[0]), WGetString(argv[1])));
		}

		template <Collection collection>
		static WObj* collection_str(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			constexpr bool isTuple = collection == Collection::Tuple;
			EXPECT_ARG_COUNT(1);
			if constexpr (isTuple) {
				EXPECT_ARG_TYPE_TUPLE(0);
			} else {
				EXPECT_ARG_TYPE_LIST(0);
			}

			auto it = std::find(context->reprStack.rbegin(), context->reprStack.rend(), argv[0]);
			if (it != context->reprStack.rend()) {
				return WCreateString(context, isTuple ? "(...)" : "[...]");
			} else {
				context->reprStack.push_back(argv[0]);
				const auto& buf = argv[0]->Get<std::vector<WObj*>>();
				std::string s(1, isTuple ? '(' : '[');
				for (WObj* child : buf) {
					WObj* v = WConvertToString(child);
					if (v == nullptr) {
						context->reprStack.pop_back();
						return nullptr;
					}
					s += v->Get<std::string>() + ", ";
				}
				context->reprStack.pop_back();
				if (!buf.empty()) {
					s.pop_back();
					s.pop_back();
				}
				if (isTuple && buf.size() == 1)
					s.push_back(',');
				s += (isTuple ? ')' : ']');
				return WCreateString(context, s.c_str());
			}
		}

		template <Collection collection>
		static WObj* collection_getindex(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(1);
			if constexpr (collection == Collection::List) {
				EXPECT_ARG_TYPE_LIST(0);
			} else {
				EXPECT_ARG_TYPE_TUPLE(0);
			}

			wint i = argv[1]->Get<wint>();
			if (i < 0 || i >= (wint)argv[0]->Get<std::vector<WObj*>>().size()) {
				//... Index out of range exception
				WUNREACHABLE();
			}

			return argv[0]->Get<std::vector<WObj*>>()[i];
		}

		static WObj* list_setindex(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(3);
			EXPECT_ARG_TYPE_LIST(0);
			EXPECT_ARG_TYPE_INT(1);

			wint i = argv[1]->Get<wint>();
			if (i < 0 || i >= (wint)argv[0]->Get<std::vector<WObj*>>().size()) {
				//... Index out of range exception
				WUNREACHABLE();
			}

			argv[0]->Get<std::vector<WObj*>>()[i] = argv[2];
			return WCreateNone(context);
		}

		static WObj* list_insertindex(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(3);
			EXPECT_ARG_TYPE_LIST(0);
			EXPECT_ARG_TYPE_INT(1);

			wint i = argv[1]->Get<wint>();
			if (i < 0 || i > (wint)argv[0]->Get<std::vector<WObj*>>().size()) {
				//... Index out of range exception
				WUNREACHABLE();
			}

			auto& buf = argv[0]->Get<std::vector<WObj*>>();
			buf.insert(buf.begin() + i, argv[2]);
			return WCreateNone(context);
		}

		static WObj* list_removeindex(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_LIST(0);
			EXPECT_ARG_TYPE_INT(1);

			wint i = argv[1]->Get<wint>();
			if (i < 0 || i >= (wint)argv[0]->Get<std::vector<WObj*>>().size()) {
				//... Index out of range exception
				WUNREACHABLE();
			}

			auto& buf = argv[0]->Get<std::vector<WObj*>>();
			buf.erase(buf.begin() + i);
			return WCreateNone(context);
		}

		template <Collection collection>
		static WObj* collection_len(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			if constexpr (collection == Collection::List) {
				EXPECT_ARG_TYPE_LIST(0);
			} else {
				EXPECT_ARG_TYPE_TUPLE(0);
			}

			return WCreateInt(context, (wint)argv[0]->Get<std::vector<WObj*>>().size());
		}

		static WObj* map_str(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_MAP(0);

			auto it = std::find(context->reprStack.rbegin(), context->reprStack.rend(), argv[0]);
			if (it != context->reprStack.rend()) {
				return WCreateString(context, "{...}");
			} else {
				context->reprStack.push_back(argv[0]);
				const auto& buf = argv[0]->Get<wings::WDict>();
				std::string s = "{";
				for (const auto& [key, val] : buf) {
					WObj* k = WConvertToString(key);
					if (k == nullptr) {
						context->reprStack.pop_back();
						return nullptr;
					}
					WObj* v = WConvertToString(val);
					if (k == nullptr) {
						context->reprStack.pop_back();
						return nullptr;
					}

					s += k->Get<std::string>() + ": ";
					s += v->Get<std::string>() + ", ";
				}
				context->reprStack.pop_back();
				if (!buf.empty()) {
					s.pop_back();
					s.pop_back();
				}
				return WCreateString(context, (s + "}").c_str());
			}
		}

		static WObj* func_str(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_FUNC(0);
			std::string s = "<function at " + PtrToString(argv[0]) + ">";
			return WCreateString(context, s.c_str());
		}

	} // namespace attrlib

	namespace lib {

		static WObj* print(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			std::string text;
			for (int i = 0; i < argc; i++) {
				if (WObj* s = WConvertToString(argv[i])) {
					text += WGetString(s);
				} else {
					return nullptr;
				}

				if (i < argc - 1) {
					text += ' ';
				}
			}
			text += '\n';
			WPrint(context, text.c_str(), (int)text.size());
			return WCreateNone(context);
		}

		static WObj* isinstance(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			bool ret{};
			if (WIsTuple(argv[1])) {
				const auto& buf = argv[1]->Get<std::vector<WObj*>>();
				ret = WIsInstance(argv[0], buf.data(), (int)buf.size()) != nullptr;
			} else {
				ret = WIsInstance(argv[0], argv + 1, 1) != nullptr;
			}
			return WCreateBool(context, ret);
		}

		static WObj* set_method(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			// Not callable from user code
			argv[2]->Get<WObj::Func>().isMethod = true;
			if (WIsClass(argv[0])) {
				argv[0]->Get<WObj::Class>().instanceAttributes.Set(argv[1]->Get<std::string>(), argv[2]);
			} else {
				WSetAttribute(argv[0], argv[1]->Get<std::string>().c_str(), argv[2]);
			}
			return WCreateNone(context);
		}

	} // namespace lib

	struct LibraryInitException : std::exception {};

	using WFuncSignature = WObj * (*)(WObj**, int, WObj*, WContext*);

	template <WFuncSignature fn>
	void RegisterMethod(WObj* _class, const char* name) {
		WFuncDesc wfn{};
		wfn.isMethod = true;
		wfn.prettyName = name;
		wfn.userdata = _class->context;
		wfn.fptr = [](WObj** argv, int argc, WObj* kwargs, void* userdata) {
			return fn(argv, argc, kwargs, (WContext*)userdata);
		};

		WObj* method = WCreateFunction(_class->context, &wfn);
		if (method == nullptr)
			throw LibraryInitException();

		WAddAttributeToClass(_class, name, method);
	}

	template <WFuncSignature fn>
	WObj* RegisterFunction(WContext* context, const char* name) {
		WFuncDesc wfn{};
		wfn.isMethod = true;
		wfn.prettyName = name;
		wfn.userdata = context;
		wfn.fptr = [](WObj** argv, int argc, WObj* kwargs, void* userdata) {
			return fn(argv, argc, kwargs, (WContext*)userdata);
		};

		WObj* obj = WCreateFunction(context, &wfn);
		if (obj == nullptr)
			throw LibraryInitException();
		WSetGlobal(context, name, obj);
		return obj;
	}

	WObj* CreateClass(WContext* context, const char* name) {
		WObj* _class = WCreateClass(context, name, nullptr, 0);
		if (_class == nullptr)
			throw LibraryInitException();
		return _class;
	}

	void InitLibrary(WContext* context) {
		try {
			auto getGlobal = [&](const char* name) {
				if (WObj* v = WGetGlobal(context, name)) {
					return v;
				} else {
					throw LibraryInitException();
				}
			};

			// Create object class
			context->builtins.object = Alloc(context);
			if (context->builtins.object == nullptr)
				throw LibraryInitException();
			context->builtins.object->type = "__class";
			context->builtins.object->data = new WObj::Class{ std::string("object") };
			context->builtins.object->finalizer.fptr = [](WObj* obj, void*) { delete (WObj::Class*)obj->data; };
			context->builtins.object->Get<WObj::Class>().instanceAttributes.Set("__class__", context->builtins.object);
			context->builtins.object->attributes.AddParent(context->builtins.object->Get<WObj::Class>().instanceAttributes);
			context->builtins.object->Get<WObj::Class>().ctor = [](WObj**, int, WObj*, void* ud) -> WObj* { return nullptr; };
			WSetGlobal(context, "object", context->builtins.object);

			// Create function class
			context->builtins.func = Alloc(context);
			if (context->builtins.func == nullptr)
				throw LibraryInitException();
			context->builtins.func->type = "__class";
			context->builtins.func->data = new WObj::Class{ std::string("function") };
			context->builtins.func->finalizer.fptr = [](WObj* obj, void*) { delete (WObj::Class*)obj->data; };
			context->builtins.func->Get<WObj::Class>().instanceAttributes.Set("__class__", context->builtins.func);
			context->builtins.func->attributes.AddParent(context->builtins.object->Get<WObj::Class>().instanceAttributes);
			context->builtins.func->Get<WObj::Class>().userdata = context;
			context->builtins.func->Get<WObj::Class>().ctor = [](WObj** argv, int argc, WObj* kwargs, void* ud) -> WObj* {
				WObj* fn = Alloc((WContext*)ud);
				if (fn == nullptr)
					return nullptr;
				ctors::func(&fn, 1, kwargs, nullptr);
				return fn;
			};
			RegisterMethod<attrlib::func_str>(context->builtins.func, "__str__");

			// Create tuple class
			context->builtins.tuple = Alloc(context);
			context->builtins.tuple->type = "__class";
			context->builtins.tuple->data = new WObj::Class{ std::string("tuple") };
			context->builtins.tuple->finalizer.fptr = [](WObj* obj, void*) { delete (WObj::Class*)obj->data; };
			context->builtins.tuple->Get<WObj::Class>().instanceAttributes.Set("__class__", context->builtins.tuple);
			context->builtins.tuple->attributes.AddParent(context->builtins.object->Get<WObj::Class>().instanceAttributes);
			context->builtins.tuple->Get<WObj::Class>().userdata = context;
			context->builtins.tuple->Get<WObj::Class>().ctor = [](WObj** argv, int argc, WObj* kwargs, void* ud) -> WObj* {
				WObj* fn = Alloc((WContext*)ud);
				if (fn == nullptr)
					return nullptr;
				ctors::collection<Collection::Tuple>(&fn, 1, kwargs, (WContext*)ud);
				return fn;
			};
			WSetGlobal(context, "tuple", context->builtins.tuple);
			RegisterMethod<ctors::collection<Collection::Tuple>>(context->builtins.tuple, "__init__");
			RegisterMethod<attrlib::collection_str<Collection::Tuple>>(context->builtins.tuple, "__str__");
			RegisterMethod<attrlib::collection_getindex<Collection::Tuple>>(context->builtins.tuple, "__getitem__");
			RegisterMethod<attrlib::collection_len<Collection::Tuple>>(context->builtins.tuple, "__len__");

			// Create NoneType class
			context->builtins.none = Alloc(context);
			context->builtins.none->type = "__class";
			context->builtins.none->data = new WObj::Class{ std::string("NoneType") };
			context->builtins.none->finalizer.fptr = [](WObj* obj, void*) { delete (WObj::Class*)obj->data; };
			context->builtins.none->Get<WObj::Class>().instanceAttributes.Set("__class__", context->builtins.none);
			context->builtins.none->attributes.AddParent(context->builtins.object->Get<WObj::Class>().instanceAttributes);
			context->builtins.none->Get<WObj::Class>().userdata = context;
			context->builtins.none->Get<WObj::Class>().ctor = [](WObj** argv, int argc, WObj* kwargs, void* ud) -> WObj* {
				return ((WContext*)ud)->builtins.none;
			};

			// Create None singleton
			context->builtins.none = Alloc(context);
			context->builtins.none->type = "__null";
			WSetAttribute(context->builtins.none, "__class__", context->builtins.none);
			context->builtins.none->attributes.AddParent(context->builtins.object->Get<WObj::Class>().instanceAttributes);

			WObj* emptyTuple = WCreateTuple(context, nullptr, 0);
			if (emptyTuple == nullptr)
				throw LibraryInitException();
			WSetAttribute(context->builtins.object, "__bases__", emptyTuple);
			WSetAttribute(context->builtins.none, "__bases__", emptyTuple);
			WSetAttribute(context->builtins.func, "__bases__", emptyTuple);
			WSetAttribute(context->builtins.tuple, "__bases__", emptyTuple);

			// Register object methods
			RegisterMethod<attrlib::object_pos>(context->builtins.object, "__pos__");
			RegisterMethod<attrlib::object_str>(context->builtins.object, "__str__");
			RegisterMethod<attrlib::object_eq>(context->builtins.object, "__eq__");
			RegisterMethod<attrlib::object_ne>(context->builtins.object, "__ne__");
			RegisterMethod<attrlib::object_iadd>(context->builtins.object, "__iadd__");

			// Call bootstrap code
			RegisterFunction<lib::set_method>(context, "set_method");
			WObj* bootstrap = WCompile(context, BOOTSTRAP_CODE, "__builtins__");
			if (bootstrap == nullptr)
				throw LibraryInitException();
			if (WCall(bootstrap, nullptr, 0) == nullptr)
				throw LibraryInitException();

			// Add native methods to builtin classes

			context->builtins._bool = getGlobal("bool");
			RegisterMethod<ctors::_bool>(context->builtins._bool, "__init__");
			RegisterMethod<attrlib::bool_nonzero>(context->builtins._bool, "__nonzero__");
			RegisterMethod<attrlib::bool_int>(context->builtins._bool, "__int__");
			RegisterMethod<attrlib::bool_float>(context->builtins._bool, "__float__");
			RegisterMethod<attrlib::bool_eq>(context->builtins._bool, "__eq__");

			context->builtins._int = getGlobal("int");
			RegisterMethod<ctors::_int>(context->builtins._int, "__init__");
			RegisterMethod<attrlib::int_nonzero>(context->builtins._int, "__nonzero__");
			RegisterMethod<attrlib::int_int>(context->builtins._int, "__int__");
			RegisterMethod<attrlib::int_float>(context->builtins._int, "__float__");
			RegisterMethod<attrlib::int_str>(context->builtins._int, "__str__");
			RegisterMethod<attrlib::int_eq>(context->builtins._int, "__eq__");
			RegisterMethod<attrlib::int_gt>(context->builtins._int, "__gt__");
			RegisterMethod<attrlib::int_ge>(context->builtins._int, "__ge__");
			RegisterMethod<attrlib::int_lt>(context->builtins._int, "__lt__");
			RegisterMethod<attrlib::int_le>(context->builtins._int, "__le__");
			RegisterMethod<attrlib::int_neg>(context->builtins._int, "__neg__");
			RegisterMethod<attrlib::int_add>(context->builtins._int, "__add__");
			RegisterMethod<attrlib::int_sub>(context->builtins._int, "__sub__");
			RegisterMethod<attrlib::int_mul>(context->builtins._int, "__mul__");
			RegisterMethod<attrlib::int_truediv>(context->builtins._int, "__truediv__");
			RegisterMethod<attrlib::int_floordiv>(context->builtins._int, "__floordiv__");
			RegisterMethod<attrlib::int_mod>(context->builtins._int, "__mod__");
			RegisterMethod<attrlib::int_pow>(context->builtins._int, "__pow__");
			RegisterMethod<attrlib::int_and>(context->builtins._int, "__and__");
			RegisterMethod<attrlib::int_or>(context->builtins._int, "__or__");
			RegisterMethod<attrlib::int_xor>(context->builtins._int, "__xor__");
			RegisterMethod<attrlib::int_invert>(context->builtins._int, "__invert__");
			RegisterMethod<attrlib::int_lshift>(context->builtins._int, "__lshift__");
			RegisterMethod<attrlib::int_rshift>(context->builtins._int, "__rshift__");

			context->builtins._float = getGlobal("float");
			RegisterMethod<ctors::_float>(context->builtins._float, "__init__");
			RegisterMethod<attrlib::float_nonzero>(context->builtins._float, "__nonzero__");
			RegisterMethod<attrlib::float_int>(context->builtins._float, "__int__");
			RegisterMethod<attrlib::float_float>(context->builtins._float, "__float__");
			RegisterMethod<attrlib::float_str>(context->builtins._float, "__str__");
			RegisterMethod<attrlib::float_eq>(context->builtins._float, "__eq__");
			RegisterMethod<attrlib::float_neg>(context->builtins._float, "__neg__");
			RegisterMethod<attrlib::float_add>(context->builtins._float, "__add__");
			RegisterMethod<attrlib::float_sub>(context->builtins._float, "__sub__");
			RegisterMethod<attrlib::float_mul>(context->builtins._float, "__mul__");
			RegisterMethod<attrlib::float_truediv>(context->builtins._float, "__truediv__");
			RegisterMethod<attrlib::float_floordiv>(context->builtins._float, "__floordiv__");
			RegisterMethod<attrlib::float_mod>(context->builtins._float, "__mod__");
			RegisterMethod<attrlib::float_pow>(context->builtins._float, "__pow__");

			context->builtins.str = getGlobal("str");
			RegisterMethod<ctors::str>(context->builtins.str, "__init__");
			RegisterMethod<attrlib::str_int>(context->builtins.str, "__int__");
			RegisterMethod<attrlib::str_float>(context->builtins.str, "__float__");

			context->builtins.list = getGlobal("list");
			RegisterMethod<ctors::collection<Collection::List>>(context->builtins.list, "__init__");
			RegisterMethod<attrlib::collection_str<Collection::List>>(context->builtins.list, "__str__");
			RegisterMethod<attrlib::collection_getindex<Collection::List>>(context->builtins.list, "__getindex");
			RegisterMethod<attrlib::list_setindex>(context->builtins.list, "__setindex");
			RegisterMethod<attrlib::collection_len<Collection::List>>(context->builtins.list, "__len__");
			RegisterMethod<attrlib::list_insertindex>(context->builtins.list, "__insertindex");
			RegisterMethod<attrlib::list_removeindex>(context->builtins.list, "__removeindex");

			context->builtins.dict = getGlobal("dict");
			RegisterMethod<ctors::map>(context->builtins.dict, "__init__");
			RegisterMethod<attrlib::map_str>(context->builtins.dict, "__str__");

			// Add native free functions
			RegisterFunction<lib::print>(context, "print");
			context->builtins.isinstance = RegisterFunction<lib::isinstance>(context, "isinstance");

			// Call the rest of the non-native library code
			WObj* builtins = WCompile(context, LIBRARY_CODE, "__builtins__");
			if (builtins == nullptr)
				throw LibraryInitException();
			if (WCall(builtins, nullptr, 0) == nullptr)
				throw LibraryInitException();

			std::vector<std::string> toDelete;
			for (const auto& [k, v] : context->globals)
				if (WIsNone(*v))
					toDelete.push_back(k);
			for (const auto& s : toDelete)
				WDeleteGlobal(context, s.c_str());

			context->builtins.slice = getGlobal("slice");
			context->builtins.baseException = getGlobal("BaseException");
			context->builtins.exception = getGlobal("Exception");
			context->builtins.syntaxError = getGlobal("SyntaxError");
			context->builtins.nameError = getGlobal("NameError");
			context->builtins.typeError = getGlobal("TypeError");
			context->builtins.valueError = getGlobal("ValueError");
			context->builtins.attributeError = getGlobal("AttributeError");
			context->builtins.lookupError = getGlobal("LookupError");
			context->builtins.indexError = getGlobal("IndexError");
			context->builtins.keyError = getGlobal("KeyError");
			context->builtins.arithmeticError = getGlobal("ArithmeticError");
			context->builtins.overflowError = getGlobal("OverflowError");
			context->builtins.zeroDivisionError = getGlobal("ZeroDivisionError");
		} catch (LibraryInitException&) {
			std::abort(); // Internal error
		}
	}
} // namespace wings
