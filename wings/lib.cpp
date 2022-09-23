#include "impl.h"
#include "gc.h"
#include <iostream>
#include <sstream>
#include <unordered_set>
#include <cmath>
#include <bit>
#include <algorithm>
#include <queue>
#include <optional>

static const char* const LIBRARY_CODE = R"(
class __DefaultIter:
	def __init__(self, iterable):
		self.iterable = iterable
		self.i = 0
	def __next__(self):
		try:
			val = self.iterable[self.i]
		except IndexError:
			raise StopIteration
		self.i += 1
		return val
	def __iter__(self):
		return self

class __DefaultReverseIter:
	def __init__(self, iterable):
		self.iterable = iterable
		self.i = len(iterable) - 1
	def __next__(self):
		if self.i >= 0:
			val = self.iterable[self.i]
			self.i -= 1
			return val
		raise StopIteration
	def __iter__(self):
		return self

class __RangeIter:
	def __init__(self, start, stop, step):
		self.cur = start
		self.stop = stop
		self.step = step
	def __next__(self):
		cur = self.cur
		if self.step > 0:
			if cur >= self.stop:
				raise StopIteration
		else:
			if cur <= self.stop:
				raise StopIteration
		self.cur = cur + self.step
		return cur
	def __iter__(self):
		return self

class range:
	def __init__(self, start, stop=None, step=None):
		if step is 0:
			raise ValueError("step cannot be 0")
		if stop == None:
			if not isinstance(start, int):
				raise TypeError("stop must be an integer")
			self.start = 0
			self.stop = start
			self.step = 1
		elif step is None:
			if not isinstance(start, int):
				raise TypeError("start must be an integer")
			elif not isinstance(stop, int):
				raise TypeError("start must be an integer")
			self.start = start
			self.stop = stop
			self.step = 1
		else:
			if not isinstance(start, int):
				raise TypeError("start must be an integer")
			elif not isinstance(stop, int):
				raise TypeError("start must be an integer")
			elif not isinstance(step, int):
				raise TypeError("step must be an integer")
			self.start = start
			self.stop = stop
			self.step = step
	def __iter__(self):
		return __RangeIter(self.start, self.stop, self.step)
		
class slice:
	def __init__(self, start, stop=None, step=None):
		if stop is None and step is None:
			self.start = None
			self.stop = start
			self.step = None
		elif step is None:
			self.start = start
			self.stop = stop
			self.step = None
		else:
			self.start = start
			self.stop = stop
			self.step = step

def sorted(iterable, reverse=False):
	li = list(iterable)
	li.sort(reverse=reverse) 
	return li

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

static bool ParseIndex(WObj* container, WObj* index, wint& out, std::optional<wint>& size) {
	WObj* len = WLen(container);
	if (len == nullptr)
		return false;

	if (!WIsInt(index)) {
		WRaiseTypeError(container->context, "index must be an integer");
		return false;
	}

	wint length = size.has_value() ? size.value() : WGetInt(len);
	wint i = WGetInt(index);

	if (i < 0) {
		out = length + i;
	} else {
		out = i;
	}
	return true;
}

static bool ParseIndex(WObj* container, WObj* index, wint& out) {
	std::optional<wint> size;
	return ParseIndex(container, index, out, size);
}

template <class F>
static bool IterateRange(wint start, wint stop, wint step, F f) {
	WASSERT(step);
	if (step > 0) {
		for (wint i = (wint)start; i < (wint)stop; i += step)
			if (!f(i))
				return false;
	} else {
		for (wint i = (wint)start; i > (wint)stop; i += step)
			if (!f(i))
				return false;
	}
	return true;
}

static bool ParseSlice(WObj* container, WObj* slice, wint& start, wint& stop, wint& step) {
	std::optional<wint> size;
	std::vector<wings::WObjRef> refs;
	refs.emplace_back(container);
	refs.emplace_back(slice);

	WObj* stepAttr = WGetAttribute(slice, "step");
	refs.emplace_back(stepAttr);
	if (stepAttr == nullptr) {
		return false;
	} else if (WIsNone(stepAttr)) {
		step = 1;
	} else if (!WIsInt(stepAttr)) {
		WRaiseTypeError(slice->context, "slice step attribute must be an integer");
		return false;
	} else if ((step = WGetInt(stepAttr)) == 0) {
		WRaiseValueError(slice->context, "slice step cannot be 0");
		return false;
	}

	WObj* startAttr = WGetAttribute(slice, "start");
	refs.emplace_back(startAttr);
	bool hasStart = true;
	if (startAttr == nullptr) {
		return false;
	} else if (WIsNone(startAttr)) {
		hasStart = false;
	} else if (!ParseIndex(container, startAttr, start, size)) {
		return false;
	}

	WObj* stopAttr = WGetAttribute(slice, "stop");
	refs.emplace_back(stopAttr);
	bool hasStop = true;
	if (stopAttr == nullptr) {
		return false;
	} else if (WIsNone(stopAttr)) {
		hasStop = false;
	} else if (!ParseIndex(container, stopAttr, stop, size)) {
		return false;
	}

	auto getSize = [&](wint& out) {
		if (size.has_value()) {
			out = size.value();
		} else {
			WObj* len = WLen(container);
			if (len == nullptr)
				return false;
			out = WGetInt(len);
			size = out;
		}
		return true;
	};

	if (!hasStart) {
		if (step < 0) {
			if (!getSize(start))
				return false;
			start--;
		} else {
			start = 0;
		}
	}

	if (!hasStop) {
		if (step < 0) {
			stop = -1;
		} else {
			if (!getSize(stop))
				return false;
		}
	}

	return true;
}

static void StringReplace(std::string& str, std::string_view from, std::string_view to, wint count) {
	if (from.empty())
		return;
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos && count > 0) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length();
		count--;
	}
}

static std::vector<std::string> StringSplit(std::string s, std::string_view sep, wint maxSplit) {
	std::vector<std::string> buf;
	size_t pos = 0;
	std::string token;
	while ((pos = s.find(sep)) != std::string::npos && maxSplit > 0) {
		token = s.substr(0, pos);
		if (!token.empty())
			buf.push_back(std::move(token));
		s.erase(0, pos + sep.size());
		maxSplit--;
	}
	if (!s.empty())
		buf.push_back(std::move(s));
	return buf;
}

static std::vector<std::string> StringSplitChar(std::string s, std::string_view chars, wint maxSplit) {
	size_t last = 0;
	size_t next = 0;
	std::vector<std::string> buf;
	while ((next = s.find_first_of(chars, last)) != std::string::npos && maxSplit > 0) {
		if (next > last)
			buf.push_back(s.substr(last, next - last));
		last = next + 1;
		maxSplit--;
	}
	if (last < s.size())
		buf.push_back(s.substr(last));
	return buf;
}

static std::vector<std::string> StringSplitLines(std::string s, bool keepLineBreaks) {
	size_t last = 0;
	size_t next = 0;
	std::vector<std::string> buf;
	while ((next = s.find_first_of("\r\n", last)) != std::string::npos) {
		buf.push_back(s.substr(last, next - last));
		last = next + 1;
		if (s[next] == '\r' && next + 1 < s.size() && s[next + 1] == '\n')
			last++;
	}
	if (last < s.size())
		buf.push_back(s.substr(last));
	return buf;
}

static bool IsSpace(char c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r'
		|| c == '\v' || c == '\f';
};

static bool MergeSort(WObj** data, size_t len, WObj* key) {
	if (len == 1)
		return true;

	WObj** left = data;
	size_t leftSize = len / 2;
	WObj** right = data + leftSize;
	size_t rightSize = len - leftSize;
	if (!MergeSort(left, leftSize, key))
		return false;
	if (!MergeSort(right, rightSize, key))
		return false;

	std::vector<WObj*> buf(len);
	size_t a = 0;
	size_t b = 0;
	for (size_t i = 0; i < len; i++) {
		if (a == leftSize) {
			// No more elements on the left
			buf[i] = right[b];
			b++;
		} else if (b == rightSize) {
			// No more elements on the right
			buf[i] = left[a];
			a++;
		} else {
			WObj* leftMapped = key ? WCall(key, &left[a], 1) : left[a];
			if (leftMapped == nullptr)
				return false;
			WObj* rightMapped = key ? WCall(key, &right[b], 1) : right[b];
			if (rightMapped == nullptr)
				return false;

			WObj* gt = WLessThan(rightMapped, leftMapped);
			if (gt == nullptr)
				return false;

			if (WGetBool(gt)) {
				// right < left
				buf[i] = right[b];
				b++;
			} else {
				// right >= left
				buf[i] = left[a];
				a++;
			}
		}
	}
	
	for (size_t i = 0; i < len; i++)
		data[i] = buf[i];
	return true;
}

#define EXPECT_ARG_COUNT(n) do if (argc != n) { WRaiseArgumentCountError(context, argc, n); return nullptr; } while (0)
#define EXPECT_ARG_COUNT_AT_LEAST(n) do if (argc < n) { WRaiseArgumentCountError(context, argc, n); return nullptr; } while (0)
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
#define EXPECT_ARG_TYPE_SET(index) EXPECT_ARG_TYPE(index, WIsSet, "set");
#define EXPECT_ARG_TYPE_FUNC(index) EXPECT_ARG_TYPE(index, WIsFunction, "function");

namespace wings {

	namespace ctors {

		static WObj* object(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			argv[0]->attributes = context->builtins.object->Get<WObj::Class>().instanceAttributes.Copy();
			argv[0]->type = "__object";
			return WCreateNone(context);
		}

		static WObj* _bool(WObj** argv, int argc, WObj* kwargs, void* ud) {
			WContext* context = (WContext*)ud;
			EXPECT_ARG_COUNT_BETWEEN(0, 1); // Called without self

			if (argc == 1) {
				WObj* res = WCallMethod(argv[0], "__nonzero__", nullptr, 0);
				if (res == nullptr) {
					return nullptr;
				} else if (!WIsBool(res)) {
					WRaiseTypeError(context, "__nonzero__() returned a non bool type");
					return nullptr;
				}
				return res;
			}

			return context->builtins._false;
		}

		static WObj* _int(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT_BETWEEN(1, 3);

			wint v = 0;
			if (argc >= 2) {
				WObj* res = WCallMethod(argv[1], "__int__", argv + 2, argc - 2);
				if (res == nullptr) {
					return nullptr;
				} else if (!WIsInt(res)) {
					WRaiseTypeError(context, "__int__() returned a non int type");
					return nullptr;
				}
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

			wfloat v = 0;
			if (argc == 2) {
				WObj* res = WCallMethod(argv[1], "__float__", nullptr, 0);
				if (res == nullptr) {
					return nullptr;
				} else if (!WIsIntOrFloat(res)) {
					WRaiseTypeError(context, "__float__() returned a non float type");
					return nullptr;
				}
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

			const char* v = "";
			if (argc == 2) {
				WObj* res = WCallMethod(argv[1], "__str__", nullptr, 0);
				if (res == nullptr) {
					return nullptr;
				} else if (!WIsString(res)) {
					WRaiseTypeError(context, "__str__() returned a non string type");
					return nullptr;
				}
				v = WGetString(res);
			}
			argv[0]->attributes = context->builtins.str->Get<WObj::Class>().instanceAttributes.Copy();
			argv[0]->type = "__str";
			argv[0]->data = new std::string(v);
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

		static WObj* set(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);

			argv[0]->attributes = context->builtins.set->Get<WObj::Class>().instanceAttributes.Copy();
			argv[0]->type = "__set";
			argv[0]->data = new wings::WSet();
			argv[0]->finalizer.fptr = [](WObj* obj, void*) { delete (wings::WSet*)obj->data; };

			return WCreateNone(context);
		}

		static WObj* func(WObj** argv, int argc, WObj* kwargs, void* ud) {
			// Not callable from user code

			//argv[0]->attributes = ((WContext*)ud)->builtins.func->Get<WObj::Class>().instanceAttributes.Copy();
			argv[0]->type = "__func";
			argv[0]->data = new WObj::Func();
			argv[0]->finalizer.fptr = [](WObj* obj, void*) { delete (WObj::Func*)obj->data; };

			return nullptr;
			//return WCreateNone(context);
		}

		static WObj* BaseException(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT_BETWEEN(1, 2);
			if (argc == 2) {
				WSetAttribute(argv[0], "_message", argv[1]);
				return WCreateNone(context);
			} else if (WObj* msg = WCreateString(context)) {
				WSetAttribute(argv[0], "_message", msg);
				return WCreateNone(context);
			} else {
				return nullptr;
			}
		}

		static WObj* DictIter(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_MAP(1);
			auto* it = new WDict::iterator(argv[1]->Get<WDict>().begin());
			WSetUserdata(argv[0], it);
			argv[0]->finalizer.fptr = [](WObj* obj, void*) { delete (WDict::iterator*)obj->data; };
			WLinkReference(argv[0], argv[1]);
			return WCreateNone(context);
		}
		
	} // namespace ctors

	namespace methods {

		static WObj* object_pos(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			return argv[0];
		}

		static WObj* object_str(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			std::string s = "<" + WObjTypeToString(argv[0]) + " object at 0x" + PtrToString(argv[0]) + ">";
			return WCreateString(context, s.c_str());
		}

		static WObj* object_nonzero(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			return WCreateBool(context, true);
		}

		static WObj* object_repr(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			return WConvertToString(argv[0]);
		}

		static WObj* object_eq(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			return WCreateBool(context, argv[0] == argv[1]);
		}

		static WObj* object_ne(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			WObj* eq = WEquals(argv[0], argv[1]);
			if (eq == nullptr)
				return nullptr;
			return WCreateBool(context, !WGetBool(eq));
		}

		static WObj* object_le(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			WObj* lt = WLessThan(argv[0], argv[1]);
			if (lt == nullptr)
				return nullptr;
			if (WGetBool(lt))
				return WCreateBool(context, true);			
			return WEquals(argv[0], argv[1]);
		}

		static WObj* object_ge(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			WObj* lt = WLessThan(argv[0], argv[1]);
			if (lt == nullptr)
				return nullptr;
			return WCreateBool(context, !WGetBool(lt));
		}

		static WObj* object_gt(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			WObj* lt = WLessThan(argv[0], argv[1]);
			if (lt == nullptr)
				return nullptr;
			if (WGetBool(lt))
				return WCreateBool(context, false);

			WObj* eq = WEquals(argv[0], argv[1]);
			if (eq == nullptr)
				return nullptr;
			return WCreateBool(context, !WGetBool(eq));
		}

		static WObj* object_hash(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			wint hash = (wint)std::hash<WObj*>()(argv[0]);
			return WCreateInt(context, hash);
		}

		static WObj* object_iadd(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			return WCallMethod(argv[0], "__add__", &argv[1], 1);
		}

		static WObj* object_isub(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			return WCallMethod(argv[0], "__sub__", &argv[1], 1);
		}

		static WObj* object_imul(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			return WCallMethod(argv[0], "__mul__", &argv[1], 1);
		}

		static WObj* object_itruediv(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			return WCallMethod(argv[0], "__truediv__", &argv[1], 1);
		}

		static WObj* object_ifloordiv(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			return WCallMethod(argv[0], "__floordiv__", &argv[1], 1);
		}

		static WObj* object_imod(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			return WCallMethod(argv[0], "__mod__", &argv[1], 1);
		}

		static WObj* object_ipow(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			return WCallMethod(argv[0], "__pow__", &argv[1], 1);
		}

		static WObj* object_iand(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			return WCallMethod(argv[0], "__and__", &argv[1], 1);
		}

		static WObj* object_ior(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			return WCallMethod(argv[0], "__or__", &argv[1], 1);
		}

		static WObj* object_ixor(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			return WCallMethod(argv[0], "__xor__", &argv[1], 1);
		}

		static WObj* object_ilshift(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			return WCallMethod(argv[0], "__lshift__", &argv[1], 1);
		}

		static WObj* object_irshift(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			return WCallMethod(argv[0], "__rshift__", &argv[1], 1);
		}

		static WObj* object_iter(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			return WCall(context->builtins.defaultIter, argv, 1);
		}

		static WObj* object_reversed(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			return WCall(context->builtins.defaultReverseIter, argv, 1);
		}

		static WObj* null_nonzero(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_NULL(0);
			return WCreateNone(context);
		}

		static WObj* null_str(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_NULL(0);
			return WCreateString(context, "None");
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

		static WObj* bool_str(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_BOOL(0);
			return WCreateString(context, WGetBool(argv[0]) ? "True" : "False");
		}

		static WObj* bool_eq(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_BOOL(0);
			return WCreateBool(context, WIsBool(argv[1]) && WGetBool(argv[0]) == WGetBool(argv[1]));
		}

		static WObj* bool_hash(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_BOOL(0);
			wint hash = (wint)std::hash<bool>()(WGetBool(argv[0]));
			return WCreateInt(context, hash);
		}

		static WObj* bool_abs(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_BOOL(0);
			return WCreateInt(context, WGetBool(argv[0]) ? 1 : 0);
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

		static WObj* int_lt(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return WCreateBool(context, WGetFloat(argv[0]) < WGetFloat(argv[1]));
		}

		static WObj* int_hash(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_INT(0);
			wint hash = (wint)std::hash<wint>()(WGetInt(argv[0]));
			return WCreateInt(context, hash);
		}

		static WObj* int_abs(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_INT(0);
			return WCreateInt(context, std::abs(WGetInt(argv[0])));
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
				WRaiseValueError(context, "Shift cannot be negative");
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
				WRaiseValueError(context, "Shift cannot be negative");
				return nullptr;
			}
			shift = std::min(shift, (wint)sizeof(wint) * 8);
			return WCreateInt(context, WGetInt(argv[0]) >> shift);
		}

		static WObj* int_bit_length(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_INT(0);

			wuint n = (wuint)WGetInt(argv[0]);
			return WCreateInt(context, (wint)std::bit_width(n));
		}

		static WObj* int_bit_count(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_INT(0);

			wuint n = (wuint)WGetInt(argv[0]);
			return WCreateInt(context, (wint)std::popcount(n));
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

		static WObj* float_lt(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return WCreateBool(context, WGetFloat(argv[0]) < WGetFloat(argv[1]));
		}

		static WObj* float_hash(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_FLOAT(0);
			wint hash = (wint)std::hash<wfloat>()(WGetFloat(argv[0]));
			return WCreateInt(context, hash);
		}

		static WObj* float_abs(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_FLOAT(0);
			return WCreateFloat(context, std::abs(WGetFloat(argv[0])));
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

		static WObj* float_is_integer(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_FLOAT(0);

			wfloat f = WGetFloat(argv[0]);
			return WCreateBool(context, std::floor(f) == f);
		}

		static WObj* str_nonzero(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_STRING(0);
			std::string s = WGetString(argv[0]);
			return WCreateBool(context, !s.empty());
		}

		static WObj* str_int(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT_BETWEEN(1, 2);
			EXPECT_ARG_TYPE_STRING(0);

			constexpr std::string_view DIGITS = "0123456789abcdefghijklmnopqrstuvwxyz";

			auto isDigit = [&](char c, int base = 10) {
				auto sub = DIGITS.substr(0, base);
				return sub.find(std::tolower(c)) != std::string_view::npos;
			};

			auto digitValueOf = [](char c, int base) {
				return DIGITS.substr(0, base).find(std::tolower(c));
			};

			std::string s = WGetString(argv[0]);
			const char* p = s.c_str();

			std::optional<int> expectedBase;
			if (argc == 2) {
				expectedBase = (int)WGetInt(argv[1]);
			}

			int base = 10;
			if (expectedBase.has_value()) {
				base = expectedBase.value();
			} else if (*p == '0') {
				switch (p[1]) {
				case 'b': case 'B': base = 2; break;
				case 'o': case 'O': base = 8; break;
				case 'x': case 'X': base = 16; break;
				}

				if (base != 10) {
					p += 2;
					if (!isDigit(*p, base)) {
						switch (base) {
						case 2: WRaiseValueError(context, "Invalid binary string"); return nullptr;
						case 8: WRaiseValueError(context, "Invalid octal string"); return nullptr;
						case 16: WRaiseValueError(context, "Invalid hexadecimal string"); return nullptr;
						default: WUNREACHABLE();
						}
					}
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
				WRaiseValueError(context, "Invalid integer string");
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
				case 'o': case 'O': base = 8; break;
				case 'x': case 'X': base = 16; break;
				}
			}

			if (base != 10) {
				p += 2;
				if (!isDigit(*p, base) && *p != '.') {
					switch (base) {
					case 2: WRaiseValueError(context, "Invalid binary string"); return nullptr;
					case 8: WRaiseValueError(context, "Invalid octal string"); return nullptr;
					case 16: WRaiseValueError(context, "Invalid hexadecimal string"); return nullptr;
					default: WUNREACHABLE();
					}
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
				WRaiseValueError(context, "Invalid float string");
				return nullptr;
			}

			return WCreateFloat(context, fvalue);
		}

		static WObj* str_str(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_STRING(0);
			return argv[0];
		}

		static WObj* str_repr(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_STRING(0);

			std::string s = WGetString(argv[0]);
			return WCreateString(context, ("'" + s + "'").c_str());
		}

		static WObj* str_len(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_STRING(0);
			return WCreateInt(context, (wint)argv[0]->Get<std::string>().size());
		}

		static WObj* str_eq(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_STRING(0);
			return WCreateBool(context, WIsString(argv[1]) && std::strcmp(WGetString(argv[0]), WGetString(argv[1])) == 0);
		}

		static WObj* str_lt(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_STRING(0);
			EXPECT_ARG_TYPE_STRING(1);
			return WCreateBool(context, std::strcmp(WGetString(argv[0]), WGetString(argv[1])) < 0);
		}

		static WObj* str_hash(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_STRING(0);
			wint hash = (wint)std::hash<std::string_view>()(WGetString(argv[0]));
			return WCreateInt(context, hash);
		}

		static WObj* str_add(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_STRING(0);
			EXPECT_ARG_TYPE_STRING(1);
			std::string s = WGetString(argv[0]);
			s += WGetString(argv[1]);
			return WCreateString(context, s.c_str());
		}

		static WObj* str_mul(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_STRING(0);
			EXPECT_ARG_TYPE_INT(1);
			wint multiplier = WGetInt(argv[1]);
			std::string_view arg = WGetString(argv[0]);
			std::string s;
			s.reserve(arg.size() * (size_t)multiplier);
			for (wint i = 0; i < multiplier; i++)
				s += arg;
			return WCreateString(context, s.c_str());
		}

		static WObj* str_contains(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_STRING(0);
			EXPECT_ARG_TYPE_STRING(1);
			return WCreateBool(context, std::strstr(WGetString(argv[0]), WGetString(argv[1])));
		}

		static WObj* str_getitem(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_STRING(0);

			if (WIsInt(argv[1])) {
				wint index;
				if (!ParseIndex(argv[0], argv[1], index))
					return nullptr;

				std::string_view s = WGetString(argv[0]);
				if (index < 0 || index >= (wint)s.size()) {
					WRaiseIndexError(context);
					return nullptr;
				}

				char buf[2] = { s[index], '\0' };
				return WCreateString(context, buf);
			} else if (WIsInstance(argv[1], &context->builtins.slice, 1)) {
				wint start, stop, step;
				if (!ParseSlice(argv[0], argv[1], start, stop, step))
					return nullptr;
				
				std::string_view s = WGetString(argv[0]);
				std::string sliced;
				bool success = IterateRange(start, stop, step, [&](wint i) {
					if (i >= 0 && i < (wint)s.size())
						sliced.push_back(s[i]);
					return true;
					});

				if (!success)
					return nullptr;
				
				return WCreateString(context, sliced.c_str());
			} else {
				WRaiseArgumentTypeError(context, 1, "int or slice");
				return nullptr;
			}
		}

		static WObj* str_capitalize(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_STRING(0);
			std::string s = WGetString(argv[0]);
			if (!s.empty())
				s[0] = (char)std::toupper(s[0]);
			return WCreateString(context, s.c_str());
		}

		static WObj* str_lower(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_STRING(0);

			std::string s = WGetString(argv[0]);
			std::transform(s.begin(), s.end(), s.begin(), std::tolower);
			return WCreateString(context, s.c_str());
		}

		static WObj* str_upper(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_STRING(0);

			std::string s = WGetString(argv[0]);
			std::transform(s.begin(), s.end(), s.begin(), std::toupper);
			return WCreateString(context, s.c_str());
		}

		static WObj* str_casefold(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			return str_lower(argv, argc, kwargs, context);
		}

		static WObj* str_center(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT_BETWEEN(2, 3);
			EXPECT_ARG_TYPE_STRING(0);
			EXPECT_ARG_TYPE_INT(1);
			if (argc >= 3) EXPECT_ARG_TYPE_STRING(2);
			
			const char* fill = argc == 3 ? WGetString(argv[2]) : " ";
			if (std::strlen(fill) != 1) {
				WRaiseTypeError(context, "The fill character must be exactly one character long");
				return nullptr;
			}

			std::string s = WGetString(argv[0]);
			wint desiredLen = WGetInt(argv[1]);
			while (true) {
				if ((wint)s.size() >= desiredLen)
					break;
				s.push_back(fill[0]);
				if ((wint)s.size() >= desiredLen)
					break;
				s.insert(s.begin(), fill[0]);
			}

			return WCreateString(context, s.c_str());
		}

		static WObj* str_count(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_STRING(0);
			EXPECT_ARG_TYPE_STRING(1);
			
			std::string_view s = WGetString(argv[0]);
			std::string_view search = WGetString(argv[1]);			
			wint count = 0;
			size_t pos = 0;
			while ((pos = s.find(search, pos)) != std::string_view::npos) {
				count++;
				pos += search.size();
			}

			return WCreateInt(context, count);
		}

		static WObj* str_format(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT_AT_LEAST(1);
			EXPECT_ARG_TYPE_STRING(0);
			
			const char* fmt = WGetString(argv[0]);
			enum class Mode { Null, Auto, Manual } mode = Mode::Null;
			size_t autoIndex = 0;
			std::string s;
			for (auto p = fmt; *p; ++p) {
				if (*p != '{') {
					s += *p;
					continue;
				}

				size_t index = 0;
				bool useAutoIndexing = true;
				++p;
				while (*p != '}') {
					if (*p >= '0' && *p <= '9') {
						index = 10 * index + ((size_t)*p - '0');
						useAutoIndexing = false;
						++p;
					} else {
						WRaiseValueError(context, "Invalid format string");
						return nullptr;
					}
				}

				if (useAutoIndexing) {
					if (mode == Mode::Manual) {
						WRaiseValueError(
							context,
							"cannot switch from automatic field numbering to manual field specification"
						);
						return nullptr;
					}
					mode = Mode::Auto;
					index = autoIndex;
					autoIndex++;
				} else {
					if (mode == Mode::Auto) {
						WRaiseValueError(
							context,
							"cannot switch from automatic field numbering to manual field specification"
						);
						return nullptr;
					}
					mode = Mode::Manual;
				}

				if ((int)index >= argc - 1) {
					WRaiseIndexError(context);
					return nullptr;
				}

				WObj* item = WConvertToString(argv[index + 1]);
				if (item == nullptr)
					return nullptr;
				s += WGetString(item);
			}

			return WCreateString(context, s.c_str());
		}

		static WObj* str_startswith(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_STRING(0);
			EXPECT_ARG_TYPE_STRING(1);

			std::string_view s = WGetString(argv[0]);
			std::string_view end = WGetString(argv[1]);
			return WCreateBool(context, s.starts_with(end));
		}

		static WObj* str_endswith(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_STRING(0);
			EXPECT_ARG_TYPE_STRING(1);

			std::string_view s = WGetString(argv[0]);
			std::string_view end = WGetString(argv[1]);
			return WCreateBool(context, s.ends_with(end));
		}

		template <bool reverse>
		static WObj* str_findx(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT_BETWEEN(2, 4);
			EXPECT_ARG_TYPE_STRING(0);
			EXPECT_ARG_TYPE_STRING(1);
			
			wint start = 0;
			std::optional<wint> size;
			if (argc >= 3) {
				EXPECT_ARG_TYPE_INT(2);
				if (!ParseIndex(argv[0], argv[2], start, size))
					return nullptr;
			}

			wint end = 0;
			if (argc >= 4) {
				EXPECT_ARG_TYPE_INT(3);
				if (!ParseIndex(argv[0], argv[3], end, size))
					return nullptr;
			} else {
				WObj* len = WLen(argv[0]);
				if (len == nullptr)
					return nullptr;
				end = (size_t)WGetInt(len);
			}
			
			std::string_view s = WGetString(argv[0]);
			std::string_view find = WGetString(argv[1]);
			
			wint substrSize = end - start;
			size_t location;
			if (substrSize < 0) {
				location = std::string_view::npos;
			} else {
				start = std::clamp(start, (wint)0, (wint)s.size());
				if (reverse) {
					location = s.substr(start, (size_t)substrSize).rfind(find);
				} else {
					location = s.substr(start, (size_t)substrSize).find(find);
				}
			}
			
			if (location == std::string_view::npos) {
				return WCreateInt(context, -1);
			} else {
				return WCreateInt(context, (wint)location);
			}
		}

		template <bool reverse>
		static WObj* str_indexx(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			WObj* location = str_findx<reverse>(argv, argc, kwargs, context);
			if (location == nullptr)
				return nullptr;
			
			if (WGetInt(location) == -1) {
				WRaiseValueError(context, "substring not found");
				return nullptr;
			} else {
				return location;
			}
		}

		static WObj* str_find(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			return str_findx<false>(argv, argc, kwargs, context);
		}

		static WObj* str_index(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			return str_indexx<false>(argv, argc, kwargs, context);
		}

		static WObj* str_rfind(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			return str_findx<true>(argv, argc, kwargs, context);
		}

		static WObj* str_rindex(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			return str_indexx<true>(argv, argc, kwargs, context);
		}

		template <auto F>
		static WObj* str_isx(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_STRING(0);

			std::string_view s = WGetString(argv[0]);
			return WCreateBool(context, std::all_of(s.begin(), s.end(), F));
		}

		static WObj* str_isalnum(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			constexpr auto f = [](char c) { return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9'); };
			return str_isx<f>(argv, argc, kwargs, context);
		}

		static WObj* str_isalpha(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			constexpr auto f = [](char c) { return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z'); };
			return str_isx<f>(argv, argc, kwargs, context);
		}

		static WObj* str_isascii(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			constexpr auto f = [](char c) { return c < 128; };
			return str_isx<f>(argv, argc, kwargs, context);
		}

		static WObj* str_isdigit(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			constexpr auto f = [](char c) { return '0' <= c && c <= '9'; };
			return str_isx<f>(argv, argc, kwargs, context);
		}

		static WObj* str_isdecimal(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			return str_isdigit(argv, argc, kwargs, context);
		}

		static WObj* str_isnumeric(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			return str_isdigit(argv, argc, kwargs, context);
		}

		static WObj* str_isprintable(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			constexpr auto f = [](char c) { return c >= 32 && c <= 127; };
			return str_isx<f>(argv, argc, kwargs, context);
		}

		static WObj* str_isspace(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			return str_isx<IsSpace>(argv, argc, kwargs, context);
		}

		static WObj* str_isupper(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			constexpr auto f = [](char c) { return !('a' <= c && c <= 'z'); };
			return str_isx<f>(argv, argc, kwargs, context);
		}

		static WObj* str_islower(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			constexpr auto f = [](char c) { return !('A' <= c && c <= 'Z'); };
			return str_isx<f>(argv, argc, kwargs, context);
		}

		static WObj* str_isidentifier(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_STRING(0);

			std::string_view s = WGetString(argv[0]);
			constexpr auto f = [](char c) { return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9') || c == '_'; };
			bool allAlphaNum = std::all_of(s.begin(), s.end(), f);
			return WCreateBool(context, allAlphaNum && (s.empty() || s[0] < '0' || s[0] > '9'));
		}

		static WObj* str_join(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_STRING(0);

			struct State {
				std::string_view sep;
				std::string s;
			} state = { WGetString(argv[0]) };

			bool success = WIterate(argv[1], &state, [](WObj* obj, void* ud) {
				State& state = *(State*)ud;
				WContext* context = obj->context;

				if (!WIsString(obj)) {
					WRaiseTypeError(context, "sequence item must be a string");
					return false;
				}
				
				state.s += WGetString(obj);
				state.s += state.sep;
				return true;
			});

			if (!success)
				return nullptr;

			if (!state.s.empty())
				state.s.erase(state.s.end() - state.sep.size(), state.s.end());

			return WCreateString(context, state.s.c_str());
		}

		static WObj* str_replace(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT_BETWEEN(3, 4);
			EXPECT_ARG_TYPE_STRING(0);
			EXPECT_ARG_TYPE_STRING(1);
			EXPECT_ARG_TYPE_STRING(2);

			wint count = std::numeric_limits<wint>::max();
			if (argc == 4) {
				EXPECT_ARG_TYPE_INT(3);
				count = WGetInt(argv[3]);
			}

			std::string s = WGetString(argv[0]);
			std::string_view find = WGetString(argv[1]);
			std::string_view repl = WGetString(argv[2]);
			StringReplace(s, find, repl, count);
			return WCreateString(context, s.c_str());
		}

		template <bool left, bool zfill = false>
		static WObj* str_just(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			if constexpr (zfill) {
				EXPECT_ARG_COUNT(2);
			} else {
				EXPECT_ARG_COUNT_BETWEEN(2, 3);
			}
			EXPECT_ARG_TYPE_STRING(0);
			EXPECT_ARG_TYPE_INT(1);

			char fill = ' ';
			if constexpr (!zfill) {
				if (argc == 3) {
					EXPECT_ARG_TYPE_STRING(0);
					std::string_view fillStr = WGetString(argv[2]);
					if (fillStr.size() != 1) {
						WRaiseTypeError(context, "The fill character must be exactly one character long");
						return nullptr;
					}
					fill = fillStr[0];
				}
			} else {
				fill = '0';
			}

			std::string s = WGetString(argv[0]);

			wint len = WGetInt(argv[1]);
			if (len < (wint)s.size())
				return argv[0];

			if (left) {
				s += std::string((size_t)len - s.size(), fill);
			} else {
				s = s + std::string((size_t)len - s.size(), fill);
			}
			return WCreateString(context, s.c_str());
		}

		static WObj* str_ljust(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			return str_just<true>(argv, argc, kwargs, context);
		}

		static WObj* str_rjust(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			return str_just<false>(argv, argc, kwargs, context);
		}

		static WObj* str_zfill(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			return str_just<true, true>(argv, argc, kwargs, context);
		}

		static WObj* str_lstrip(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT_BETWEEN(1, 2);
			EXPECT_ARG_TYPE_STRING(0);

			std::string_view chars = " ";
			if (argc == 2 && !WIsNone(argv[1])) {
				EXPECT_ARG_TYPE_STRING(1);
				chars = WGetString(argv[1]);
			}

			std::string_view s = WGetString(argv[0]);
			size_t pos = s.find_first_not_of(chars);
			if (pos == std::string::npos)
				return WCreateString(context);
			return WCreateString(context, s.data() + pos);
		}

		static WObj* str_rstrip(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT_BETWEEN(1, 2);
			EXPECT_ARG_TYPE_STRING(0);

			std::string_view chars = " ";
			if (argc == 2 && !WIsNone(argv[1])) {
				EXPECT_ARG_TYPE_STRING(1);
				chars = WGetString(argv[1]);
			}

			std::string s = WGetString(argv[0]);
			size_t pos = s.find_last_not_of(chars);
			if (pos == std::string::npos)
				return WCreateString(context);
			s.erase(s.begin() + pos + 1, s.end());
			return WCreateString(context, s.c_str());
		}

		static WObj* str_strip(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT_BETWEEN(1, 2);
			EXPECT_ARG_TYPE_STRING(0);

			std::string_view chars = " ";
			if (argc == 2 && !WIsNone(argv[1])) {
				EXPECT_ARG_TYPE_STRING(1);
				chars = WGetString(argv[1]);
			}

			std::string s = WGetString(argv[0]);
			size_t pos = s.find_last_not_of(chars);
			if (pos == std::string::npos)
				return WCreateString(context);
			s.erase(s.begin() + pos + 1, s.end());

			pos = s.find_first_not_of(chars);
			if (pos == std::string::npos)
				return WCreateString(context);
			return WCreateString(context, s.data() + pos);
		}

		static WObj* str_split(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT_BETWEEN(1, 3);
			EXPECT_ARG_TYPE_STRING(0);

			wint maxSplit = -1;
			if (argc == 3) {
				EXPECT_ARG_TYPE_INT(2);
				maxSplit = WGetInt(argv[2]);
			}
			if (maxSplit == -1)
				maxSplit = std::numeric_limits<wint>::max();

			std::vector<std::string> strings;
			if (argc >= 2) {
				EXPECT_ARG_TYPE_STRING(1);
				strings = StringSplit(WGetString(argv[0]), WGetString(argv[1]), maxSplit);
			} else {
				strings = StringSplitChar(WGetString(argv[0]), " \t\n\r\v\f", maxSplit);
			}

			WObj* li = WCreateList(context);
			if (li == nullptr)
				return nullptr;
			WObjRef ref(li);

			for (const auto& s : strings) {
				WObj* str = WCreateString(context, s.c_str());
				if (str == nullptr)
					return nullptr;
				li->Get<std::vector<WObj*>>().push_back(str);
			}
			return li;
		}

		static WObj* str_splitlines(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT_BETWEEN(1, 2);
			EXPECT_ARG_TYPE_STRING(0);

			bool keepLineBreaks = false;
			if (argc == 2) {
				EXPECT_ARG_TYPE_BOOL(1);
				keepLineBreaks = WGetBool(argv[1]);
			}

			std::vector<std::string> strings = StringSplitLines(WGetString(argv[0]), keepLineBreaks);

			WObj* li = WCreateList(context);
			if (li == nullptr)
				return nullptr;
			WObjRef ref(li);

			for (const auto& s : strings) {
				WObj* str = WCreateString(context, s.c_str());
				if (str == nullptr)
					return nullptr;
				li->Get<std::vector<WObj*>>().push_back(str);
			}
			return li;
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
					WObj* v = WRepr(child);
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
		static WObj* collection_nonzero(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			if constexpr (collection == Collection::List) {
				EXPECT_ARG_TYPE_LIST(0);
			} else {
				EXPECT_ARG_TYPE_TUPLE(0);
			}

			return WCreateBool(context, !argv[0]->Get<std::vector<WObj*>>().empty());
		}

		template <Collection collection>
		static WObj* collection_lt(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			if constexpr (collection == Collection::List) {
				EXPECT_ARG_TYPE_LIST(0);
				EXPECT_ARG_TYPE_LIST(1);
			} else {
				EXPECT_ARG_TYPE_TUPLE(0);
				EXPECT_ARG_TYPE_TUPLE(1);
			}

			auto& buf1 = argv[0]->Get<std::vector<WObj*>>();
			auto& buf2 = argv[1]->Get<std::vector<WObj*>>();

			size_t minSize = buf1.size() < buf2.size() ? buf1.size() : buf2.size();

			for (size_t i = 0; i < minSize; i++) {
				WObj* lt = WLessThan(buf1[i], buf2[i]);
				if (lt == nullptr)
					return nullptr;

				if (WGetBool(lt))
					return lt;

				WObj* gt = WLessThan(buf1[i], buf2[i]);
				if (gt == nullptr)
					return nullptr;

				if (WGetBool(gt))
					return WCreateBool(context, false);
			}

			return WCreateBool(context, buf1.size() < buf2.size());
		}

		template <Collection collection>
		static WObj* collection_eq(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			if constexpr (collection == Collection::List) {
				EXPECT_ARG_TYPE_LIST(0);
				if (!WIsInstance(argv[1], &context->builtins.list, 1))
					return WCreateBool(context, false);
			} else {
				EXPECT_ARG_TYPE_TUPLE(0);
				if (!WIsInstance(argv[1], &context->builtins.tuple, 1))
					return WCreateBool(context, false);
			}

			auto& buf1 = argv[0]->Get<std::vector<WObj*>>();
			auto& buf2 = argv[1]->Get<std::vector<WObj*>>();

			if (buf1.size() != buf2.size())
				return WCreateBool(context, false);

			for (size_t i = 0; i < buf1.size(); i++) {
				if (WObj* eq = WEquals(buf1[i], buf2[i])) {
					if (!WGetBool(eq))
						return eq;
				} else {
					return nullptr;
				}
			}

			return WCreateBool(context, true);
		}

		template <Collection collection>
		static WObj* collection_contains(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			if constexpr (collection == Collection::List) {
				EXPECT_ARG_TYPE_LIST(0);
			} else {
				EXPECT_ARG_TYPE_TUPLE(0);
			}

			auto& buf = argv[0]->Get<std::vector<WObj*>>();
			for (size_t i = 0; i < buf.size(); i++) {
				if (WObj* eq = WEquals(buf[i], argv[1])) {
					if (WGetBool(eq))
						return eq;
				} else {
					return nullptr;
				}
			}

			return WCreateBool(context, false);
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

		template <Collection collection>
		static WObj* collection_count(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			if constexpr (collection == Collection::List) {
				EXPECT_ARG_TYPE_LIST(0);
			} else {
				EXPECT_ARG_TYPE_TUPLE(0);
			}

			auto& buf = argv[0]->Get<std::vector<WObj*>>();
			wint count = 0;
			for (size_t i = 0; i < buf.size(); i++) {
				WObj* eq = WEquals(argv[1], buf[i]);
				if (eq == nullptr)
					return nullptr;
				if (WGetBool(eq))
					count++;
			}

			return WCreateInt(context, count);
		}

		template <Collection collection>
		static WObj* collection_index(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			if constexpr (collection == Collection::List) {
				EXPECT_ARG_TYPE_LIST(0);
			} else {
				EXPECT_ARG_TYPE_TUPLE(0);
			}

			auto& buf = argv[0]->Get<std::vector<WObj*>>();
			for (size_t i = 0; i < buf.size(); i++) {
				WObj* eq = WEquals(argv[1], buf[i]);
				if (eq == nullptr)
					return nullptr;
				if (WGetBool(eq))
					return WCreateInt(context, (wint)i);
			}

			WRaiseValueError(context, "Value was not found");
			return nullptr;
		}

		template <Collection collection>
		static WObj* collection_getitem(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			if constexpr (collection == Collection::List) {
				EXPECT_ARG_TYPE_LIST(0);
			} else {
				EXPECT_ARG_TYPE_TUPLE(0);
			}

			if (WIsInt(argv[1])) {
				wint index;
				if (!ParseIndex(argv[0], argv[1], index))
					return nullptr;

				auto& buf = argv[0]->Get<std::vector<WObj*>>();
				if (index < 0 || index >= (wint)buf.size()) {
					WRaiseIndexError(context);
					return nullptr;
				}

				return buf[index];
			} else if (WIsInstance(argv[1], &context->builtins.slice, 1)) {
				wint start, stop, step;
				if (!ParseSlice(argv[0], argv[1], start, stop, step))
					return nullptr;

				auto& buf = argv[0]->Get<std::vector<WObj*>>();
				std::vector<WObj*> sliced;
				bool success = IterateRange(start, stop, step, [&](wint i) {
					if (i >= 0 && i < (wint)buf.size())
						sliced.push_back(buf[i]);
					return true;
					});

				if (!success)
					return nullptr;

				if constexpr (collection == Collection::List) {
					return WCreateList(context, sliced.data(), (int)sliced.size());
				} else {
					return WCreateTuple(context, sliced.data(), (int)sliced.size());
				}
			} else {
				WRaiseArgumentTypeError(context, 1, "int or slice");
				return nullptr;
			}
		}

		static WObj* list_setitem(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(3);
			EXPECT_ARG_TYPE_LIST(0);
			EXPECT_ARG_TYPE_INT(1);
			
			wint index;
			if (!ParseIndex(argv[0], argv[1], index))
				return nullptr;

			auto& buf = argv[0]->Get<std::vector<WObj*>>();
			if (index < 0 || index >= (wint)buf.size()) {
				WRaiseIndexError(context);
				return nullptr;
			}

			buf[index] = argv[2];
			return WCreateNone(context);
		}
		
		static WObj* list_append(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_LIST(0);

			argv[0]->Get<std::vector<WObj*>>().push_back(argv[1]);
			return WCreateNone(context);
		}

		static WObj* list_insert(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(3);
			EXPECT_ARG_TYPE_LIST(0);
			EXPECT_ARG_TYPE_INT(1);

			wint index;
			if (!ParseIndex(argv[0], argv[1], index))
				return nullptr;

			auto& buf = argv[0]->Get<std::vector<WObj*>>();			
			index = std::clamp(index, (wint)0, (wint)buf.size() + 1);
			buf.insert(buf.begin() + index, argv[2]);
			return WCreateNone(context);
		}

		static WObj* list_pop(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT_BETWEEN(1, 2);
			EXPECT_ARG_TYPE_LIST(0);

			auto& buf = argv[0]->Get<std::vector<WObj*>>();
			wint index = (wint)buf.size() - 1;
			if (argc == 2) {
				EXPECT_ARG_TYPE_INT(1);
				if (!ParseIndex(argv[0], argv[1], index))
					return nullptr;
			}

			if (index < 0 || index >= (wint)buf.size()) {
				WRaiseIndexError(context);
				return nullptr;
			}
			
			WObj* popped = buf[index];
			buf.erase(buf.begin() + index);
			return popped;
		}

		static WObj* list_remove(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_LIST(0);

			auto& buf = argv[0]->Get<std::vector<WObj*>>();
			for (size_t i = 0; i < buf.size(); i++) {
				WObj* eq = WEquals(argv[1], buf[i]);
				if (eq == nullptr)
					return nullptr;
				
				if (WGetBool(eq)) {
					if (i < buf.size())
						buf.erase(buf.begin() + i);
					return WCreateNone(context);
				}
			}

			WRaiseValueError(context, "Value was not found");
			return nullptr;
		}

		static WObj* list_clear(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_LIST(0);

			argv[0]->Get<std::vector<WObj*>>().clear();
			return WCreateNone(context);
		}

		static WObj* list_copy(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_LIST(0);

			auto& buf = argv[0]->Get<std::vector<WObj*>>();
			return WCreateList(context, buf.data(), !buf.size());
		}

		static WObj* list_extend(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_LIST(0);

			auto& buf = argv[0]->Get<std::vector<WObj*>>();

			if (argv[0] == argv[1]) {
				// Double the list instead of going into an infinite loop
				buf.insert(buf.end(), buf.begin(), buf.end());
			} else {
				bool success = WIterate(argv[1], &buf, [](WObj* value, void* ud) {
					std::vector<WObj*>& buf = *(std::vector<WObj*>*)ud;
					buf.push_back(value);
					return true;
					});
				if (!success)
					return nullptr;
			}
			
			return WCreateNone(context);
		}

		static WObj* list_sort(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_LIST(0);

			WObj* kw[2]{};
			const char* keys[2] = { "reverse", "key" };
			if (!WParseKwargs(kwargs, keys, 2, kw))
				return nullptr;

			bool reverse = false;
			if (kw[0] != nullptr) {
				WObj* reverseValue = WConvertToBool(kw[0]);
				if (reverseValue == nullptr)
					return nullptr;
				reverse = WGetBool(reverseValue);
			}

			std::vector<WObj*> buf = argv[0]->Get<std::vector<WObj*>>();
			std::vector<WObjRef> refs;
			for (WObj* v : buf)
				refs.emplace_back(v);

			if (!MergeSort(buf.data(), buf.size(), kw[1]))
				return nullptr;

			if (reverse)
				std::reverse(buf.begin(), buf.end());
			
			argv[0]->Get<std::vector<WObj*>>() = std::move(buf);

			return WCreateNone(context);
		}

		static WObj* list_reverse(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_LIST(0);

			auto& buf = argv[0]->Get<std::vector<WObj*>>();
			std::reverse(buf.begin(), buf.end());
			return WCreateNone(context);
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
					WObj* k = WRepr(key);
					if (k == nullptr) {
						context->reprStack.pop_back();
						return nullptr;
					}
					WObj* v = WRepr(val);
					if (v == nullptr) {
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

		static WObj* map_nonzero(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_MAP(0);
			return WCreateBool(context, !argv[0]->Get<WDict>().empty());
		}

		static WObj* map_len(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_MAP(0);
			return WCreateInt(context, (wint)argv[0]->Get<WDict>().size());
		}

		static WObj* map_contains(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_MAP(0);
			try {
				return WCreateBool(context, argv[0]->Get<WDict>().contains(argv[1]));
			} catch (HashException&) {
				return nullptr;
			}
		}

		static WObj* map_iter(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_MAP(0);
			return WCall(context->builtins.dictKeysIter, argv, 1, nullptr);
		}

		static WObj* map_values(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_MAP(0);
			return WCall(context->builtins.dictValuesIter, argv, 1, nullptr);
		}

		static WObj* map_items(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_MAP(0);
			return WCall(context->builtins.dictItemsIter, argv, 1, nullptr);
		}

		static WObj* map_get(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT_BETWEEN(2, 3);
			EXPECT_ARG_TYPE_MAP(0);

			auto& buf = argv[0]->Get<WDict>();
			WDict::iterator it;
			try {
				it = buf.find(argv[1]);
			} catch (HashException&) {
				return nullptr;
			}

			if (it == buf.end()) {
				return argc == 3 ? argv[2] : WCreateNone(context);
			}

			return it->second;
		}

		static WObj* map_getitem(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_MAP(0);

			auto& buf = argv[0]->Get<WDict>();
			WDict::iterator it;
			try {
				it = buf.find(argv[1]);
			} catch (HashException&) {
				return nullptr;
			}

			if (it == buf.end()) {
				WRaiseKeyError(context, argv[1]);
				return nullptr;
			}

			return it->second;
		}

		static WObj* map_setitem(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(3);
			EXPECT_ARG_TYPE_MAP(0);

			try {
				argv[0]->Get<WDict>()[argv[1]] = argv[2];
			} catch (HashException&) {
				return nullptr;
			}
			return WCreateNone(context);
		}

		static WObj* map_clear(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_MAP(0);
			argv[0]->Get<WDict>().clear();
			return WCreateNone(context);
		}

		static WObj* map_copy(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_MAP(0);

			std::vector<WObj*> keys;
			std::vector<WObj*> values;
			for (const auto& [k, v] : argv[0]->Get<WDict>()) {
				keys.push_back(k);
				values.push_back(v);
			}
			return WCreateDictionary(context, keys.data(), values.data(), (int)keys.size());
		}

		static WObj* map_pop(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT_BETWEEN(2, 3);
			EXPECT_ARG_TYPE_MAP(0);

			if (auto popped = argv[0]->Get<WDict>().erase(argv[1]))
				return popped.value();

			if (argc == 3)
				return argv[2];

			WRaiseKeyError(context, argv[1]);
			return nullptr;
		}

		static WObj* map_popitem(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_MAP(0);

			auto& buf = argv[0]->Get<WDict>();
			if (buf.empty()) {
				WRaiseKeyError(context);
				return nullptr;
			}

			auto popped = buf.pop();
			WObj* tupElems[2] = { popped.first, popped.second };
			return WCreateTuple(context, tupElems, 2);
		}

		static WObj* set_str(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_SET(0);

			auto it = std::find(context->reprStack.rbegin(), context->reprStack.rend(), argv[0]);
			if (it != context->reprStack.rend()) {
				return WCreateString(context, "{...}");
			} else {
				context->reprStack.push_back(argv[0]);
				const auto& buf = argv[0]->Get<wings::WSet>();

				if (buf.empty()) {
					context->reprStack.pop_back();
					return WCreateString(context, "set()");
				}

				std::string s = "{";
				for (WObj* val : buf) {
					WObj* v = WRepr(val);
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
				return WCreateString(context, (s + "}").c_str());
			}
		}

		static WObj* set_add(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_SET(0);
			argv[0]->Get<wings::WSet>().insert(argv[1]);
			return WCreateNone(context);
		}

		static WObj* func_str(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_FUNC(0);
			std::string s = "<function at " + PtrToString(argv[0]) + ">";
			return WCreateString(context, s.c_str());
		}

		static WObj* BaseException_str(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			return WGetAttribute(argv[0], "_message");
		}

		static WObj* DictKeysIter_next(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			void* data{};
			if (!WTryGetUserdata(argv[0], "__DictKeysIter", &data)) {
				WRaiseArgumentTypeError(context, 0, "__DictKeysIter");
				return nullptr;
			}
			
			auto& it = *(WDict::iterator*)data;
			if (it == WDict::iterator{}) {
				WRaiseStopIteration(context);
				return nullptr;
			}
			
			WObj* key = it->first;
			++it;
			return key;
		}

		static WObj* DictValuesIter_next(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			void* data{};
			if (!WTryGetUserdata(argv[0], "__DictValuesIter", &data)) {
				WRaiseArgumentTypeError(context, 0, "__DictValuesIter");
				return nullptr;
			}

			auto& it = *(WDict::iterator*)data;
			if (it == WDict::iterator{}) {
				WRaiseStopIteration(context);
				return nullptr;
			}

			WObj* value = it->second;
			++it;
			return value;
		}

		static WObj* DictItemsIter_next(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			void* data{};
			if (!WTryGetUserdata(argv[0], "__DictItemsIter", &data)) {
				WRaiseArgumentTypeError(context, 0, "__DictItemsIter");
				return nullptr;
			}

			auto& it = *(WDict::iterator*)data;
			if (it == WDict::iterator{}) {
				WRaiseStopIteration(context);
				return nullptr;
			}

			WObj* tup[2] = { it->first, it->second };
			++it;
			return WCreateTuple(context, tup, 2);
		}
		
		static WObj* self(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			return argv[0];
		}

	} // namespace methods

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

		static WObj* len(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			WObj* res = WCallMethod(argv[0], "__len__", nullptr, 0);
			if (res == nullptr) {
				return nullptr;
			} else if (!WIsInt(res)) {
				WRaiseTypeError(context, "__len__() returned a non int type");
				return nullptr;
			} else if (WGetInt(res) < 0) {
				WRaiseValueError(context, "__len__() returned a negative number");
				return nullptr;
			}
			return res;
		}

		static WObj* repr(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			WObj* res = WCallMethod(argv[0], "__repr__", nullptr, 0);
			if (res == nullptr) {
				return nullptr;
			} else if (!WIsString(res)) {
				WRaiseTypeError(context, "__repr__() returned a non string type");
				return nullptr;
			}
			return res;
		}

		static WObj* next(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			return WCallMethod(argv[0], "__next__", nullptr, 0);
		}

		static WObj* iter(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			return WCallMethod(argv[0], "__iter__", nullptr, 0);
		}

		static WObj* reversed(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			return WCallMethod(argv[0], "__reversed__", nullptr, 0);
		}

		static WObj* abs(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			return WCallMethod(argv[0], "__abs__", nullptr, 0);
		}

		static WObj* hash(WObj** argv, int argc, WObj* kwargs, WContext* context) {
			EXPECT_ARG_COUNT(1);
			WObj* res = WCallMethod(argv[0], "__hash__", nullptr, 0);
			if (res == nullptr) {
				return nullptr;
			} else if (!WIsInt(res)) {
				WRaiseTypeError(context, "__hash__() returned a non int type");
				return nullptr;
			}
			return res;
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

		if (WIsClass(_class)) {
			WAddAttributeToClass(_class, name, method);
		} else {
			WSetAttribute(_class, name, method);
		}
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
				if (WObj* v = WGetGlobal(context, name))
					return v;
				throw LibraryInitException();
			};

			auto createClass = [&](const char* name, WObj* base = nullptr, bool assign = true) {
				if (WObj* v = WCreateClass(context, name, &base, base ? 1 : 0)) {
					if (assign)
						WSetGlobal(context, name, v);
					return v;
				}
				throw LibraryInitException();
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
			context->builtins.object->Get<WObj::Class>().userdata = context;
			context->builtins.object->Get<WObj::Class>().ctor = [](WObj**, int, WObj* kwargs, void* ud) -> WObj* {
				WObj* obj = Alloc((WContext*)ud);
				if (obj == nullptr)
					return nullptr;
				ctors::object(&obj, 1, kwargs, (WContext*)ud);
				return obj;
			};
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
			RegisterMethod<methods::func_str>(context->builtins.func, "__str__");

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
			RegisterMethod<methods::collection_str<Collection::Tuple>>(context->builtins.tuple, "__str__");
			RegisterMethod<methods::collection_getitem<Collection::Tuple>>(context->builtins.tuple, "__getitem__");
			RegisterMethod<methods::collection_len<Collection::Tuple>>(context->builtins.tuple, "__len__");
			RegisterMethod<methods::collection_contains<Collection::Tuple>>(context->builtins.tuple, "__contains__");
			RegisterMethod<methods::collection_eq<Collection::Tuple>>(context->builtins.tuple, "__eq__");
			RegisterMethod<methods::collection_lt<Collection::Tuple>>(context->builtins.tuple, "__lt__");
			RegisterMethod<methods::collection_nonzero<Collection::Tuple>>(context->builtins.tuple, "__nonzero__");
			RegisterMethod<methods::collection_count<Collection::Tuple>>(context->builtins.tuple, "count");
			RegisterMethod<methods::collection_index<Collection::Tuple>>(context->builtins.tuple, "index");

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
			RegisterMethod<methods::null_nonzero>(context->builtins.none, "__nonzero__");
			RegisterMethod<methods::null_str>(context->builtins.none, "__str__");

			// Add __bases__ tuple to the classes created before
			WObj* emptyTuple = WCreateTuple(context, nullptr, 0);
			if (emptyTuple == nullptr)
				throw LibraryInitException();
			WSetAttribute(context->builtins.object, "__bases__", emptyTuple);
			WSetAttribute(context->builtins.none, "__bases__", emptyTuple);
			WSetAttribute(context->builtins.func, "__bases__", emptyTuple);
			WSetAttribute(context->builtins.tuple, "__bases__", emptyTuple);

			// Add methods
			RegisterMethod<methods::object_pos>(context->builtins.object, "__pos__");
			RegisterMethod<methods::object_str>(context->builtins.object, "__str__");
			RegisterMethod<methods::object_nonzero>(context->builtins.object, "__nonzero__");
			RegisterMethod<methods::object_repr>(context->builtins.object, "__repr__");
			RegisterMethod<methods::object_eq>(context->builtins.object, "__eq__");
			RegisterMethod<methods::object_ne>(context->builtins.object, "__ne__");
			RegisterMethod<methods::object_le>(context->builtins.object, "__le__");
			RegisterMethod<methods::object_gt>(context->builtins.object, "__gt__");
			RegisterMethod<methods::object_ge>(context->builtins.object, "__ge__");
			RegisterMethod<methods::object_iadd>(context->builtins.object, "__iadd__");
			RegisterMethod<methods::object_isub>(context->builtins.object, "__isub__");
			RegisterMethod<methods::object_imul>(context->builtins.object, "__imul__");
			RegisterMethod<methods::object_itruediv>(context->builtins.object, "__itruediv__");
			RegisterMethod<methods::object_ifloordiv>(context->builtins.object, "__ifloordiv__");
			RegisterMethod<methods::object_imod>(context->builtins.object, "__imod__");
			RegisterMethod<methods::object_ipow>(context->builtins.object, "__ipow__");
			RegisterMethod<methods::object_iand>(context->builtins.object, "__iand__");
			RegisterMethod<methods::object_ior>(context->builtins.object, "__ior__");
			RegisterMethod<methods::object_ixor>(context->builtins.object, "__ixor__");
			RegisterMethod<methods::object_ilshift>(context->builtins.object, "__ilshift__");
			RegisterMethod<methods::object_irshift>(context->builtins.object, "__irshift__");
			RegisterMethod<methods::object_hash>(context->builtins.object, "__hash__");
			RegisterMethod<methods::object_iter>(context->builtins.object, "__iter__");
			RegisterMethod<methods::object_reversed>(context->builtins.object, "__reversed__");

			context->builtins._bool = createClass("bool");
			context->builtins._bool->Get<WObj::Class>().ctor = ctors::_bool;
			RegisterMethod<methods::bool_nonzero>(context->builtins._bool, "__nonzero__");
			RegisterMethod<methods::bool_int>(context->builtins._bool, "__int__");
			RegisterMethod<methods::bool_float>(context->builtins._bool, "__float__");
			RegisterMethod<methods::bool_str>(context->builtins._bool, "__str__");
			RegisterMethod<methods::bool_eq>(context->builtins._bool, "__eq__");
			RegisterMethod<methods::bool_hash>(context->builtins._bool, "__hash__");
			RegisterMethod<methods::bool_abs>(context->builtins._bool, "__abs__");

			WObj* _false = Alloc(context);
			if (_false == nullptr)
				throw LibraryInitException();
			_false->attributes = context->builtins._bool->Get<WObj::Class>().instanceAttributes.Copy();
			_false->type = "__bool";
			_false->data = new bool(false);
			_false->finalizer.fptr = [](WObj* obj, void*) { delete (bool*)obj->data; };
			context->builtins._false = _false;
			WObj* _true = Alloc(context);
			if (_true == nullptr)
				throw LibraryInitException();
			_true->attributes = context->builtins._bool->Get<WObj::Class>().instanceAttributes.Copy();
			_true->type = "__bool";
			_true->data = new bool(true);
			_true->finalizer.fptr = [](WObj* obj, void*) { delete (bool*)obj->data; };
			context->builtins._true = _true;

			context->builtins._int = createClass("int");
			RegisterMethod<ctors::_int>(context->builtins._int, "__init__");
			RegisterMethod<methods::int_nonzero>(context->builtins._int, "__nonzero__");
			RegisterMethod<methods::int_int>(context->builtins._int, "__int__");
			RegisterMethod<methods::int_float>(context->builtins._int, "__float__");
			RegisterMethod<methods::int_str>(context->builtins._int, "__str__");
			RegisterMethod<methods::int_neg>(context->builtins._int, "__neg__");
			RegisterMethod<methods::int_add>(context->builtins._int, "__add__");
			RegisterMethod<methods::int_sub>(context->builtins._int, "__sub__");
			RegisterMethod<methods::int_mul>(context->builtins._int, "__mul__");
			RegisterMethod<methods::int_truediv>(context->builtins._int, "__truediv__");
			RegisterMethod<methods::int_floordiv>(context->builtins._int, "__floordiv__");
			RegisterMethod<methods::int_mod>(context->builtins._int, "__mod__");
			RegisterMethod<methods::int_pow>(context->builtins._int, "__pow__");
			RegisterMethod<methods::int_and>(context->builtins._int, "__and__");
			RegisterMethod<methods::int_or>(context->builtins._int, "__or__");
			RegisterMethod<methods::int_xor>(context->builtins._int, "__xor__");
			RegisterMethod<methods::int_invert>(context->builtins._int, "__invert__");
			RegisterMethod<methods::int_lshift>(context->builtins._int, "__lshift__");
			RegisterMethod<methods::int_rshift>(context->builtins._int, "__rshift__");
			RegisterMethod<methods::int_lt>(context->builtins._int, "__lt__");
			RegisterMethod<methods::int_eq>(context->builtins._int, "__eq__");
			RegisterMethod<methods::int_hash>(context->builtins._int, "__hash__");
			RegisterMethod<methods::int_abs>(context->builtins._int, "__abs__");
			RegisterMethod<methods::int_bit_length>(context->builtins._int, "bit_length");
			RegisterMethod<methods::int_bit_count>(context->builtins._int, "bit_count");

			context->builtins._float = createClass("float");
			RegisterMethod<ctors::_float>(context->builtins._float, "__init__");
			RegisterMethod<methods::float_nonzero>(context->builtins._float, "__nonzero__");
			RegisterMethod<methods::float_int>(context->builtins._float, "__int__");
			RegisterMethod<methods::float_float>(context->builtins._float, "__float__");
			RegisterMethod<methods::float_str>(context->builtins._float, "__str__");
			RegisterMethod<methods::float_neg>(context->builtins._float, "__neg__");
			RegisterMethod<methods::float_add>(context->builtins._float, "__add__");
			RegisterMethod<methods::float_sub>(context->builtins._float, "__sub__");
			RegisterMethod<methods::float_mul>(context->builtins._float, "__mul__");
			RegisterMethod<methods::float_truediv>(context->builtins._float, "__truediv__");
			RegisterMethod<methods::float_floordiv>(context->builtins._float, "__floordiv__");
			RegisterMethod<methods::float_mod>(context->builtins._float, "__mod__");
			RegisterMethod<methods::float_pow>(context->builtins._float, "__pow__");
			RegisterMethod<methods::float_lt>(context->builtins._float, "__lt__");
			RegisterMethod<methods::float_eq>(context->builtins._float, "__eq__");
			RegisterMethod<methods::float_hash>(context->builtins._float, "__hash__");
			RegisterMethod<methods::float_abs>(context->builtins._float, "__abs__");
			RegisterMethod<methods::float_is_integer>(context->builtins._float, "is_integer");

			context->builtins.str = createClass("str");
			RegisterMethod<ctors::str>(context->builtins.str, "__init__");
			RegisterMethod<methods::str_nonzero>(context->builtins.str, "__nonzero__");
			RegisterMethod<methods::str_int>(context->builtins.str, "__int__");
			RegisterMethod<methods::str_float>(context->builtins.str, "__float__");
			RegisterMethod<methods::str_str>(context->builtins.str, "__str__");
			RegisterMethod<methods::str_repr>(context->builtins.str, "__repr__");
			RegisterMethod<methods::str_len>(context->builtins.str, "__len__");
			RegisterMethod<methods::str_add>(context->builtins.str, "__add__");
			RegisterMethod<methods::str_mul>(context->builtins.str, "__mul__");
			RegisterMethod<methods::str_getitem>(context->builtins.str, "__getitem__");
			RegisterMethod<methods::str_contains>(context->builtins.str, "__contains__");
			RegisterMethod<methods::str_lt>(context->builtins.str, "__lt__");
			RegisterMethod<methods::str_eq>(context->builtins.str, "__eq__");
			RegisterMethod<methods::str_hash>(context->builtins.str, "__hash__");
			RegisterMethod<methods::str_capitalize>(context->builtins.str, "capitalize");
			RegisterMethod<methods::str_casefold>(context->builtins.str, "casefold");
			RegisterMethod<methods::str_lower>(context->builtins.str, "lower");
			RegisterMethod<methods::str_upper>(context->builtins.str, "upper");
			RegisterMethod<methods::str_center>(context->builtins.str, "center");
			RegisterMethod<methods::str_count>(context->builtins.str, "count");
			RegisterMethod<methods::str_format>(context->builtins.str, "format");
			RegisterMethod<methods::str_find>(context->builtins.str, "find");
			RegisterMethod<methods::str_index>(context->builtins.str, "index");
			RegisterMethod<methods::str_startswith>(context->builtins.str, "startswith");
			RegisterMethod<methods::str_endswith>(context->builtins.str, "endswith");
			RegisterMethod<methods::str_isalnum>(context->builtins.str, "isalnum");
			RegisterMethod<methods::str_isalpha>(context->builtins.str, "isalpha");
			RegisterMethod<methods::str_isascii>(context->builtins.str, "isascii");
			RegisterMethod<methods::str_isdecimal>(context->builtins.str, "isdecimal");
			RegisterMethod<methods::str_isdigit>(context->builtins.str, "isdigit");
			RegisterMethod<methods::str_isidentifier>(context->builtins.str, "isidentifier");
			RegisterMethod<methods::str_islower>(context->builtins.str, "islower");
			RegisterMethod<methods::str_isupper>(context->builtins.str, "isupper");
			RegisterMethod<methods::str_isnumeric>(context->builtins.str, "isnumeric");
			RegisterMethod<methods::str_isprintable>(context->builtins.str, "isprintable");
			RegisterMethod<methods::str_isspace>(context->builtins.str, "isspace");
			RegisterMethod<methods::str_join>(context->builtins.str, "join");
			RegisterMethod<methods::str_ljust>(context->builtins.str, "ljust");
			RegisterMethod<methods::str_lstrip>(context->builtins.str, "lstrip");
			RegisterMethod<methods::str_replace>(context->builtins.str, "replace");
			RegisterMethod<methods::str_rfind>(context->builtins.str, "rfind");
			RegisterMethod<methods::str_rindex>(context->builtins.str, "rindex");
			RegisterMethod<methods::str_rjust>(context->builtins.str, "rjust");
			RegisterMethod<methods::str_rstrip>(context->builtins.str, "rstrip");
			RegisterMethod<methods::str_split>(context->builtins.str, "split");
			RegisterMethod<methods::str_splitlines>(context->builtins.str, "splitlines");
			RegisterMethod<methods::str_strip>(context->builtins.str, "strip");
			RegisterMethod<methods::str_zfill>(context->builtins.str, "zfill");

			context->builtins.list = createClass("list");
			RegisterMethod<ctors::collection<Collection::List>>(context->builtins.list, "__init__");
			RegisterMethod<methods::collection_nonzero<Collection::List>>(context->builtins.list, "__nonzero__");
			RegisterMethod<methods::collection_str<Collection::List>>(context->builtins.list, "__str__");
			RegisterMethod<methods::collection_len<Collection::List>>(context->builtins.list, "__len__");
			RegisterMethod<methods::collection_getitem<Collection::List>>(context->builtins.list, "__getitem__");
			RegisterMethod<methods::list_setitem>(context->builtins.list, "__setitem__");
			RegisterMethod<methods::collection_contains<Collection::List>>(context->builtins.list, "__contains__");
			RegisterMethod<methods::collection_eq<Collection::List>>(context->builtins.list, "__eq__");
			RegisterMethod<methods::collection_lt<Collection::List>>(context->builtins.list, "__lt__");
			RegisterMethod<methods::collection_count<Collection::List>>(context->builtins.list, "count");
			RegisterMethod<methods::collection_index<Collection::List>>(context->builtins.list, "index");
			RegisterMethod<methods::list_append>(context->builtins.list, "append");
			RegisterMethod<methods::list_clear>(context->builtins.list, "clear");
			RegisterMethod<methods::list_copy>(context->builtins.list, "copy");
			RegisterMethod<methods::list_extend>(context->builtins.list, "extend");
			RegisterMethod<methods::list_insert>(context->builtins.list, "insert");
			RegisterMethod<methods::list_pop>(context->builtins.list, "pop");
			RegisterMethod<methods::list_remove>(context->builtins.list, "remove");
			RegisterMethod<methods::list_reverse>(context->builtins.list, "reverse");
			RegisterMethod<methods::list_sort>(context->builtins.list, "sort");

			context->builtins.dict = createClass("dict");
			RegisterMethod<ctors::map>(context->builtins.dict, "__init__");
			RegisterMethod<methods::map_nonzero>(context->builtins.dict, "__nonzero__");
			RegisterMethod<methods::map_str>(context->builtins.dict, "__str__");
			RegisterMethod<methods::map_contains>(context->builtins.dict, "__contains__");
			RegisterMethod<methods::map_getitem>(context->builtins.dict, "__getitem__");
			RegisterMethod<methods::map_iter>(context->builtins.dict, "__iter__");
			RegisterMethod<methods::map_len>(context->builtins.dict, "__len__");
			RegisterMethod<methods::map_setitem>(context->builtins.dict, "__setitem__");
			RegisterMethod<methods::map_clear>(context->builtins.dict, "clear");
			RegisterMethod<methods::map_copy>(context->builtins.dict, "copy");
			RegisterMethod<methods::map_get>(context->builtins.dict, "get");
			RegisterMethod<methods::map_iter>(context->builtins.dict, "keys");
			RegisterMethod<methods::map_values>(context->builtins.dict, "values");
			RegisterMethod<methods::map_items>(context->builtins.dict, "items");
			RegisterMethod<methods::map_pop>(context->builtins.dict, "pop");
			RegisterMethod<methods::map_popitem>(context->builtins.dict, "popitem");
			//RegisterMethod<methods::map_setdefault>(context->builtins.dict, "setdefault");
			//RegisterMethod<methods::map_update>(context->builtins.dict, "update");
			//RegisterMethod<methods::map_values>(context->builtins.dict, "values");

			context->builtins.set = createClass("set");
			RegisterMethod<ctors::set>(context->builtins.set, "__init__");
			RegisterMethod<methods::set_str>(context->builtins.set, "__str__");
			//RegisterMethod<methods::set_contains>(context->builtins.set, "__contains__");
			//RegisterMethod<methods::set_iter>(context->builtins.set, "__iter__");
			//RegisterMethod<methods::set_len>(context->builtins.set, "__len__");
			RegisterMethod<methods::set_add>(context->builtins.set, "add");
			//RegisterMethod<methods::set_clear>(context->builtins.set, "clear");
			//RegisterMethod<methods::set_copy>(context->builtins.set, "copy");
			//RegisterMethod<methods::set_difference>(context->builtins.set, "difference");
			//RegisterMethod<methods::set_discard>(context->builtins.set, "discard");
			//RegisterMethod<methods::set_intersection>(context->builtins.set, "intersection");
			//RegisterMethod<methods::set_isdisjoint>(context->builtins.set, "isdisjoint");
			//RegisterMethod<methods::set_issubset>(context->builtins.set, "issubset");
			//RegisterMethod<methods::set_issuperset>(context->builtins.set, "issuperset");
			//RegisterMethod<methods::set_pop>(context->builtins.set, "pop");
			//RegisterMethod<methods::set_remove>(context->builtins.set, "remove");
			//RegisterMethod<methods::set_symmetric_difference>(context->builtins.set, "symmetric_difference");
			//RegisterMethod<methods::set_union>(context->builtins.set, "union");
			//RegisterMethod<methods::set_update>(context->builtins.set, "update");

			context->builtins.dictKeysIter = createClass("__DictKeysIter", nullptr, false);
			RegisterMethod<ctors::DictIter>(context->builtins.dictKeysIter, "__init__");
			RegisterMethod<methods::DictKeysIter_next>(context->builtins.dictKeysIter, "__next__");
			RegisterMethod<methods::self>(context->builtins.dictKeysIter, "__iter__");

			context->builtins.dictValuesIter = createClass("__DictValuesIter", nullptr, false);
			RegisterMethod<ctors::DictIter>(context->builtins.dictValuesIter, "__init__");
			RegisterMethod<methods::DictValuesIter_next>(context->builtins.dictValuesIter, "__next__");
			RegisterMethod<methods::self>(context->builtins.dictValuesIter, "__iter__");

			context->builtins.dictItemsIter = createClass("__DictItemsIter", nullptr, false);
			RegisterMethod<ctors::DictIter>(context->builtins.dictItemsIter, "__init__");
			RegisterMethod<methods::DictItemsIter_next>(context->builtins.dictItemsIter, "__next__");
			RegisterMethod<methods::self>(context->builtins.dictItemsIter, "__iter__");

			// Add free functions
			context->builtins.isinstance = RegisterFunction<lib::isinstance>(context, "isinstance");
			context->builtins.len = RegisterFunction<lib::len>(context, "len");
			context->builtins.repr = RegisterFunction<lib::repr>(context, "repr");
			context->builtins.hash = RegisterFunction<lib::hash>(context, "hash");
			RegisterFunction<lib::print>(context, "print");
			RegisterFunction<lib::next>(context, "next");
			RegisterFunction<lib::iter>(context, "iter");
			RegisterFunction<lib::abs>(context, "abs");
			RegisterFunction<lib::reversed>(context, "reversed");

			// Create exception classes
			context->builtins.baseException = createClass("BaseException");
			RegisterMethod<ctors::BaseException>(context->builtins.baseException, "__init__");
			RegisterMethod<methods::BaseException_str>(context->builtins.baseException, "__str__");
			context->builtins.exception = createClass("Exception", context->builtins.baseException);
			context->builtins.syntaxError = createClass("SyntaxError", context->builtins.exception);
			context->builtins.nameError = createClass("NameError", context->builtins.exception);
			context->builtins.typeError = createClass("TypeError", context->builtins.exception);
			context->builtins.valueError = createClass("ValueError", context->builtins.exception);
			context->builtins.attributeError = createClass("AttributeError", context->builtins.exception);
			context->builtins.lookupError = createClass("LookupError", context->builtins.exception);
			context->builtins.indexError = createClass("IndexError", context->builtins.lookupError);
			context->builtins.keyError = createClass("KeyError", context->builtins.lookupError);
			context->builtins.arithmeticError = createClass("ArithmeticError", context->builtins.exception);
			context->builtins.overflowError = createClass("OverflowError", context->builtins.arithmeticError);
			context->builtins.zeroDivisionError = createClass("ZeroDivisionError", context->builtins.arithmeticError);
			context->builtins.stopIteration = createClass("StopIteration", context->builtins.exception);

			// Initialize the rest with a script
			WObj* lib = WCompile(context, LIBRARY_CODE, "__builtins__");
			if (lib == nullptr)
				throw LibraryInitException();
			if (WCall(lib, nullptr, 0) == nullptr)
				throw LibraryInitException();

			context->builtins.slice = getGlobal("slice");
			context->builtins.defaultIter = getGlobal("__DefaultIter");
			context->builtins.defaultReverseIter = getGlobal("__DefaultReverseIter");

		} catch (LibraryInitException&) {
			std::abort(); // Internal error
		}
	}
} // namespace wings
