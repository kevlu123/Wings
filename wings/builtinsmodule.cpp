#include "builtinsmodule.h"
#include "common.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <array>
#include <unordered_set>
#include <queue>
#include <optional>
#include <cmath>
#include <algorithm>
#include <bit>

static const char* const CODE = R"(
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

class __CodeObject:
	def __init__(self, f):
		self.f = f

class __ReadLineIter:
	def __init__(self, f):
		self.f = f
	def __next__(self):
		line = self.f.readline()
		if line == "":
			raise StopIteration
		return line
	def __iter__(self):
		return self

def abs(x):
	return x.__abs__()

def all(x):
	for v in x:
		if not v:
			return False
	return True

def any(x):
	for v in x:
		if v:
			return True
	return False

def divmod(a, b):
	return (a // b, a % b)

class enumerate:
	def __init__(self, x, start=0):
		self.iter = iter(x)
		self.i = start
	def __iter__(self):
		return self
	def __next__(self):
		i = self.i
		self.i += 1
		return (i, next(self.iter))

class filter:
	def __init__(self, f, iterable):
		self.f = f
		self.iter = iter(iterable)
	def __iter__(self):
		return self
	def __next__(self):
		while True:
			val = next(self.iter)
			if self.f(val):
				return val
		raise StopIteration

def hasattr(obj, name):
	try:
		getattr(obj, name)
		return True
	except AttributeError:
		return False

def hash(x):
	v = x.__hash__()
	if not isinstance(v, int):
		raise TypeError("__hash__() returned a non integer type")
	return v

def iter(x):
	return x.__iter__()

def len(x):
	v = x.__len__()
	if not isinstance(v, int):
		raise TypeError("__len__() returned a non integer type")
	elif v < 0:
		raise ValueError("__len__() returned a negative value")
	return v

class map:
	def __init__(self, f, iterable):
		self.f = f
		self.iter = iter(iterable)
	def __iter__(self):
		return self
	def __next__(self):
		return self.f(next(self.iter))

def max(*args, **kwargs):
	if len(args) == 1:
		args = list(args[0])
	else:
		args = list(args)

	if len(args) == 0:
		if "default" in kwargs:
			return kwargs["default"]
		raise ValueError("max() arg is an empty sequence")
		
	if "key" in kwargs:
		key = kwargs["key"]
	else:
		key = lambda x: x
	
	m = args[0]
	for i in range(1, len(args)):
		if key(args[i]) > key(m):
			m = args[i]
	return m

def min(*args, **kwargs):
	if len(args) == 1:
		args = list(args[0])
	else:
		args = list(args)

	if len(args) == 0:
		if "default" in kwargs:
			return kwargs["default"]
		raise ValueError("min() arg is an empty sequence")
		
	if "key" in kwargs:
		key = kwargs["key"]
	else:
		key = lambda x: x
	
	m = args[0]
	for i in range(1, len(args)):
		if key(args[i]) < key(m):
			m = args[i]
	return m

def next(x):
	return x.__next__()

def pow(x, y):
	return x ** y

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

def repr(x):
	v = x.__repr__()
	if not isinstance(v, str):
		raise TypeError("__repr__() returned a non string type")
	return v

def reversed(x):
	return x.__reversed__()

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
	def __index__(self):
		return self

def sorted(iterable, key=None, reverse=False):
	li = list(iterable)
	li.sort(key=key, reverse=reverse)
	return li

def sum(iterable, start=0):
	n = start
	for i in iterable:
		n += i
	return n

def type(x):
	return x.__class__

class zip:
	def __init__(self, *iterables):
		self.iters = [iter(i) for i in iterables]
	def __iter__(self):
		return self
	def __next__(self):
		return tuple([next(i) for i in self.iters])

class BaseException:
	def __init__(self, message=""):
		self._message = message
	def __str__(self):
		return self._message

class SystemExit(BaseException):
	pass

class Exception(BaseException):
	pass

class StopIteration(Exception):
	pass

class ArithmeticError(Exception):
	pass

class OverflowError(ArithmeticError):
	pass

class ZeroDivisionError(ArithmeticError):
	pass

class AttributeError(Exception):
	pass

class ImportError(Exception):
	pass

class LookupError(Exception):
	pass

class IndexError(LookupError):
	pass

class KeyError(LookupError):
	pass

class MemoryError(Exception):
	pass

class NameError(Exception):
	pass

class OSError(Exception):
	pass

class IsADirectoryError(OSError):
	pass

class RuntimeError(Exception):
	pass

class NotImplementedError(RuntimeError):
	pass

class RecursionError(RuntimeError):
	pass

class SyntaxError(Exception):
	pass

class TypeError(Exception):
	pass

class ValueError(Exception):
	pass
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

static bool AbsIndex(Wg_Obj* container, Wg_Obj* index, Wg_int& out, std::optional<Wg_int>& size) {
	Wg_Obj* len = Wg_UnaryOp(WG_UOP_LEN, container);
	if (len == nullptr)
		return false;

	if (!Wg_IsInt(index)) {
		Wg_RaiseException(container->context, WG_EXC_TYPEERROR, "index must be an integer");
		return false;
	}

	Wg_int length = size.has_value() ? size.value() : Wg_GetInt(len);
	Wg_int i = Wg_GetInt(index);

	if (i < 0) {
		out = length + i;
	} else {
		out = i;
	}
	return true;
}

static bool AbsIndex(Wg_Obj* container, Wg_Obj* index, Wg_int& out) {
	std::optional<Wg_int> size;
	return AbsIndex(container, index, out, size);
}

template <class F>
static bool IterateRange(Wg_int start, Wg_int stop, Wg_int step, F f) {
	WG_ASSERT(step);
	if (step > 0) {
		for (Wg_int i = (Wg_int)start; i < (Wg_int)stop; i += step)
			if (!f(i))
				return false;
	} else {
		for (Wg_int i = (Wg_int)start; i > (Wg_int)stop; i += step)
			if (!f(i))
				return false;
	}
	return true;
}

static bool AbsSlice(Wg_Obj* container, Wg_Obj* slice, Wg_int& start, Wg_int& stop, Wg_int& step) {
	std::optional<Wg_int> size;
	std::vector<wings::Wg_ObjRef> refs;
	refs.emplace_back(container);
	refs.emplace_back(slice);

	Wg_Obj* stepAttr = Wg_GetAttribute(slice, "step");
	refs.emplace_back(stepAttr);
	if (stepAttr == nullptr) {
		return false;
	} else if (Wg_IsNone(stepAttr)) {
		step = 1;
	} else if (!Wg_IsInt(stepAttr)) {
		Wg_RaiseException(slice->context, WG_EXC_TYPEERROR, "slice step attribute must be an integer");
		return false;
	} else if ((step = Wg_GetInt(stepAttr)) == 0) {
		Wg_RaiseException(slice->context, WG_EXC_VALUEERROR, "slice step cannot be 0");
		return false;
	}

	Wg_Obj* startAttr = Wg_GetAttribute(slice, "start");
	refs.emplace_back(startAttr);
	bool hasStart = true;
	if (startAttr == nullptr) {
		return false;
	} else if (Wg_IsNone(startAttr)) {
		hasStart = false;
	} else if (!AbsIndex(container, startAttr, start, size)) {
		return false;
	}

	Wg_Obj* stopAttr = Wg_GetAttribute(slice, "stop");
	refs.emplace_back(stopAttr);
	bool hasStop = true;
	if (stopAttr == nullptr) {
		return false;
	} else if (Wg_IsNone(stopAttr)) {
		hasStop = false;
	} else if (!AbsIndex(container, stopAttr, stop, size)) {
		return false;
	}

	auto getSize = [&](Wg_int& out) {
		if (size.has_value()) {
			out = size.value();
		} else {
			Wg_Obj* len = Wg_UnaryOp(WG_UOP_LEN, container);
			if (len == nullptr)
				return false;
			out = Wg_GetInt(len);
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

static void StringReplace(std::string& str, std::string_view from, std::string_view to, Wg_int count) {
	if (from.empty())
		return;
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos && count > 0) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length();
		count--;
	}
}

static std::vector<std::string> StringSplit(std::string s, std::string_view sep, Wg_int maxSplit) {
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

static std::vector<std::string> StringSplitChar(std::string s, std::string_view chars, Wg_int maxSplit) {
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

static bool MergeSort(Wg_Obj** data, size_t len, Wg_Obj* key) {
	if (len == 1)
		return true;

	Wg_Obj** left = data;
	size_t leftSize = len / 2;
	Wg_Obj** right = data + leftSize;
	size_t rightSize = len - leftSize;
	if (!MergeSort(left, leftSize, key))
		return false;
	if (!MergeSort(right, rightSize, key))
		return false;

	std::vector<Wg_Obj*> buf(len);
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
			Wg_Obj* leftMapped = key ? Wg_Call(key, &left[a], 1) : left[a];
			if (leftMapped == nullptr)
				return false;
			Wg_Obj* rightMapped = key ? Wg_Call(key, &right[b], 1) : right[b];
			if (rightMapped == nullptr)
				return false;

			Wg_Obj* gt = Wg_BinaryOp(WG_BOP_LE, rightMapped, leftMapped);
			if (gt == nullptr)
				return false;

			if (Wg_GetBool(gt)) {
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

namespace wings {

	namespace ctors {

		static Wg_Obj* object(Wg_Context* context, Wg_Obj** argv, int argc) { // Excludes self
			WG_EXPECT_ARG_COUNT(0);

			Wg_Obj* obj = Alloc(context);
			if (obj == nullptr)
				return nullptr;
			
			obj->attributes = context->builtins.object->Get<Wg_Obj::Class>().instanceAttributes.Copy();
			obj->type = "__object";
			return obj;
		}

		static Wg_Obj* none(Wg_Context* context, Wg_Obj** argv, int argc) { // Excludes self
			return context->builtins.none;
		}

		static Wg_Obj* _bool(Wg_Context* context, Wg_Obj** argv, int argc) { // Excludes self
			WG_EXPECT_ARG_COUNT_BETWEEN(0, 1);

			if (argc == 1) {
				Wg_Obj* res = Wg_CallMethod(argv[0], "__nonzero__", nullptr, 0);
				if (res == nullptr) {
					return nullptr;
				} else if (!Wg_IsBool(res)) {
					Wg_RaiseException(context, WG_EXC_TYPEERROR, "__nonzero__() returned a non bool type");
					return nullptr;
				}
				return res;
			}

			return context->builtins._false;
		}
		
		static Wg_Obj* _int(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 3);

			Wg_int v = 0;
			if (argc >= 2) {
				Wg_Obj* res = Wg_CallMethod(argv[1], "__int__", argv + 2, argc - 2);
				if (res == nullptr) {
					return nullptr;
				} else if (!Wg_IsInt(res)) {
					Wg_RaiseException(context, WG_EXC_TYPEERROR, "__int__() returned a non int type");
					return nullptr;
				}
				v = Wg_GetInt(res);
			}

			argv[0]->attributes = context->builtins._int->Get<Wg_Obj::Class>().instanceAttributes.Copy();
			argv[0]->type = "__int";
			argv[0]->data = new Wg_int(v);
			argv[0]->finalizer.fptr = [](Wg_Obj* obj, void*) { delete (Wg_int*)obj->data; };

			return Wg_None(context);
		}

		static Wg_Obj* _float(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 2);

			Wg_float v = 0;
			if (argc == 2) {
				Wg_Obj* res = Wg_CallMethod(argv[1], "__float__", nullptr, 0);
				if (res == nullptr) {
					return nullptr;
				} else if (!Wg_IsIntOrFloat(res)) {
					Wg_RaiseException(context, WG_EXC_TYPEERROR, "__float__() returned a non float type");
					return nullptr;
				}
				v = Wg_GetFloat(res);
			}

			argv[0]->attributes = context->builtins._float->Get<Wg_Obj::Class>().instanceAttributes.Copy();
			argv[0]->type = "__float";
			argv[0]->data = new Wg_float(v);
			argv[0]->finalizer.fptr = [](Wg_Obj* obj, void*) { delete (Wg_float*)obj->data; };

			return Wg_None(context);
		}

		static Wg_Obj* str(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 2);

			const char* v = "";
			if (argc == 2) {
				Wg_Obj* res = Wg_CallMethod(argv[1], "__str__", nullptr, 0);
				if (res == nullptr) {
					return nullptr;
				} else if (!Wg_IsString(res)) {
					Wg_RaiseException(context, WG_EXC_TYPEERROR, "__str__() returned a non string type");
					return nullptr;
				}
				v = Wg_GetString(res);
			}
			argv[0]->attributes = context->builtins.str->Get<Wg_Obj::Class>().instanceAttributes.Copy();
			argv[0]->type = "__str";
			argv[0]->data = new std::string(v);
			argv[0]->finalizer.fptr = [](Wg_Obj* obj, void*) { delete (std::string*)obj->data; };

			return Wg_None(context);
		}

		static Wg_Obj* tuple(Wg_Context* context, Wg_Obj** argv, int argc) { // Excludes self
			WG_EXPECT_ARG_COUNT_BETWEEN(0, 1);

			struct State {
				std::vector<Wg_Obj*> v;
				std::vector<Wg_ObjRef> refs;
			} s;
			if (argc == 1) {
				auto f = [](Wg_Obj* x, void* u) {
					State* s = (State*)u;
					s->refs.emplace_back(x);
					s->v.push_back(x);
					return true;
				};

				if (!Wg_Iterate(argv[0], &s, f))
					return nullptr;
			}

			Wg_Obj* obj = Alloc(context);
			if (obj == nullptr)
				return nullptr;
			
			obj->attributes = context->builtins.tuple->Get<Wg_Obj::Class>().instanceAttributes.Copy();
			obj->type = "__tuple";
			obj->data = new std::vector<Wg_Obj*>(std::move(s.v));
			obj->finalizer.fptr = [](Wg_Obj* obj, void*) { delete (std::vector<Wg_Obj*>*)obj->data; };

			return obj;
		}

		static Wg_Obj* list(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 2);

			struct State {
				std::vector<Wg_Obj*> v;
				std::vector<Wg_ObjRef> refs;
			} s;
			if (argc == 2) {
				auto f = [](Wg_Obj* x, void* u) {
					State* s = (State*)u;
					s->refs.emplace_back(x);
					s->v.push_back(x);
					return true;
				};

				if (!Wg_Iterate(argv[1], &s, f))
					return nullptr;
			}

			argv[0]->attributes = context->builtins.list->Get<Wg_Obj::Class>().instanceAttributes.Copy();
			argv[0]->type = "__list";
			argv[0]->data = new std::vector<Wg_Obj*>(std::move(s.v));
			argv[0]->finalizer.fptr = [](Wg_Obj* obj, void*) { delete (std::vector<Wg_Obj*>*)obj->data; };

			return Wg_None(context);
		}

		static Wg_Obj* map(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 2);

			WDict* buf{};
			argv[0]->attributes = context->builtins.dict->Get<Wg_Obj::Class>().instanceAttributes.Copy();
			argv[0]->type = "__map";
			argv[0]->data = buf = new wings::WDict();
			argv[0]->finalizer.fptr = [](Wg_Obj* obj, void*) { delete (wings::WDict*)obj->data; };

			if (argc == 2) {
				Wg_Obj* iterable = argv[1];
				if (Wg_IsDictionary(argv[1])) {
					iterable = Wg_CallMethod(argv[1], "items", nullptr, 0);
				}

				auto f = [](Wg_Obj* obj, void* ud) {
					Wg_Obj* kv[2]{};
					if (!Wg_Unpack(obj, kv, 2))
						return false;

					wings::Wg_ObjRef ref(kv[1]);
					try {
						((WDict*)ud)->operator[](kv[0]) = kv[1];
					} catch (HashException&) {}
					return true;
				};

				if (!Wg_Iterate(iterable, buf, f))
					return nullptr;
			}

			for (const auto& [k, v] : Wg_GetKwargs(context)->Get<WDict>()) {
				try {
					buf->operator[](k) = v;
				} catch (HashException&) {
					return nullptr;
				}
			}

			return Wg_None(context);
		}

		static Wg_Obj* set(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 2);

			WSet* buf{};
			argv[0]->attributes = context->builtins.set->Get<Wg_Obj::Class>().instanceAttributes.Copy();
			argv[0]->type = "__set";
			argv[0]->data = buf = new wings::WSet();
			argv[0]->finalizer.fptr = [](Wg_Obj* obj, void*) { delete (wings::WSet*)obj->data; };

			if (argc == 2) {
				Wg_Obj* iterable = argv[1];
				auto f = [](Wg_Obj* obj, void* ud) {
					try {
						((WSet*)ud)->insert(obj);
					} catch (HashException&) {}
					return true;
				};

				if (!Wg_Iterate(iterable, buf, f))
					return nullptr;
			}

			return Wg_None(context);
		}

		static Wg_Obj* func(Wg_Context* context, Wg_Obj** argv, int argc) { // Excludes self
			// Not callable from user code

			Wg_Obj* obj = Alloc(context);
			if (obj == nullptr)
				return nullptr;

			//obj->attributes = ((Wg_Context*)ud)->builtins.func->Get<Wg_Obj::Class>().instanceAttributes.Copy();
			obj->type = "__func";
			obj->data = new Wg_Obj::Func();
			obj->finalizer.fptr = [](Wg_Obj* obj, void*) { delete (Wg_Obj::Func*)obj->data; };

			return obj;
		}

		static Wg_Obj* BaseException(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 2);
			if (argc == 2) {
				Wg_SetAttribute(argv[0], "_message", argv[1]);
				return Wg_None(context);
			} else if (Wg_Obj* msg = Wg_NewString(context)) {
				Wg_SetAttribute(argv[0], "_message", msg);
				return Wg_None(context);
			} else {
				return nullptr;
			}
		}

		static Wg_Obj* DictIter(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_MAP(1);
			auto* it = new WDict::iterator(argv[1]->Get<WDict>().begin());
			Wg_SetUserdata(argv[0], it);
			argv[0]->finalizer.fptr = [](Wg_Obj* obj, void*) { delete (WDict::iterator*)obj->data; };
			Wg_LinkReference(argv[0], argv[1]);
			return Wg_None(context);
		}

		static Wg_Obj* SetIter(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_SET(1);
			auto* it = new WSet::iterator(argv[1]->Get<WSet>().begin());
			Wg_SetUserdata(argv[0], it);
			argv[0]->finalizer.fptr = [](Wg_Obj* obj, void*) { delete (WSet::iterator*)obj->data; };
			Wg_LinkReference(argv[0], argv[1]);
			return Wg_None(context);
		}

		static Wg_Obj* File(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(2, 3);
			WG_EXPECT_ARG_TYPE_STRING(1);

			const char* filename = Wg_GetString(argv[1]);

			std::ios::openmode mode{};
			if (argc == 3) {
				WG_EXPECT_ARG_TYPE_STRING(2);
				std::string m = Wg_GetString(argv[2]);
	
				size_t b;
				if ((b = m.find('b')) != std::string::npos) {
					mode |= std::ios::binary;
					m.erase(b);
				}
				
				if (m == "r") {
					mode = std::ios::in;
				} else if (m == "w") {
					mode = std::ios::out;
				} else if (m == "a") {
					mode = std::ios::app;
				} else if (m == "r+") {
					mode = std::ios::in | std::ios::out;
				} else if (m == "w+") {
					mode = std::ios::in | std::ios::out | std::ios::trunc;
				} else if (m == "a+") {
					mode = std::ios::in | std::ios::app;
				} else {
					Wg_RaiseException(context, WG_EXC_VALUEERROR, "Invalid file mode");
					return nullptr;
				}
			} else {
				mode = std::ios::in;
			}

			auto* f = new std::fstream(filename, mode);
			if (!f->is_open()) {
				Wg_RaiseException(context, WG_EXC_OSERROR, "Failed to open file");
				return nullptr;
			}
			Wg_SetUserdata(argv[0], f);

			Wg_FinalizerDesc fd{};
			fd.fptr = [](Wg_Obj* obj, void*) { delete (std::fstream*)obj->data; };
			Wg_SetFinalizer(argv[0], &fd);

			bool readable = mode & std::ios::in;
			bool writable = mode & std::ios::out;
			
			Wg_SetAttribute(argv[0], "_readable", Wg_NewBool(context, readable));
			Wg_SetAttribute(argv[0], "_writable", Wg_NewBool(context, writable));
			
			return Wg_None(context);
		}
		
	} // namespace ctors

	namespace methods {
		
		static Wg_Obj* object_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			std::string s = "<" + WObjTypeToString(argv[0]) + " object at 0x" + PtrToString(argv[0]) + ">";
			return Wg_NewString(context, s.c_str());
		}

		static Wg_Obj* object_nonzero(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			return Wg_NewBool(context, true);
		}

		static Wg_Obj* object_repr(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			return Wg_UnaryOp(WG_UOP_STR, argv[0]);
		}

		static Wg_Obj* object_eq(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			return Wg_NewBool(context, argv[0] == argv[1]);
		}

		static Wg_Obj* object_ne(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			Wg_Obj* eq = Wg_BinaryOp(WG_BOP_EQ, argv[0], argv[1]);
			if (eq == nullptr)
				return nullptr;
			return Wg_NewBool(context, !Wg_GetBool(eq));
		}

		static Wg_Obj* object_le(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			Wg_Obj* lt = Wg_BinaryOp(WG_BOP_LT, argv[0], argv[1]);
			if (lt == nullptr)
				return nullptr;
			if (Wg_GetBool(lt))
				return Wg_NewBool(context, true);			
			return Wg_BinaryOp(WG_BOP_EQ, argv[0], argv[1]);
		}

		static Wg_Obj* object_ge(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			Wg_Obj* lt = Wg_BinaryOp(WG_BOP_LT, argv[0], argv[1]);
			if (lt == nullptr)
				return nullptr;
			return Wg_NewBool(context, !Wg_GetBool(lt));
		}

		static Wg_Obj* object_gt(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			Wg_Obj* lt = Wg_BinaryOp(WG_BOP_LT, argv[0], argv[1]);
			if (lt == nullptr)
				return nullptr;
			if (Wg_GetBool(lt))
				return Wg_NewBool(context, false);

			Wg_Obj* eq = Wg_BinaryOp(WG_BOP_EQ, argv[0], argv[1]);
			if (eq == nullptr)
				return nullptr;
			return Wg_NewBool(context, !Wg_GetBool(eq));
		}

		static Wg_Obj* object_hash(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			Wg_int hash = (Wg_int)std::hash<Wg_Obj*>()(argv[0]);
			return Wg_NewInt(context, hash);
		}

		static Wg_Obj* object_iadd(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__add__", &argv[1], 1);
		}

		static Wg_Obj* object_isub(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__sub__", &argv[1], 1);
		}

		static Wg_Obj* object_imul(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__mul__", &argv[1], 1);
		}

		static Wg_Obj* object_itruediv(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__truediv__", &argv[1], 1);
		}

		static Wg_Obj* object_ifloordiv(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__floordiv__", &argv[1], 1);
		}

		static Wg_Obj* object_imod(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__mod__", &argv[1], 1);
		}

		static Wg_Obj* object_ipow(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__pow__", &argv[1], 1);
		}

		static Wg_Obj* object_iand(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__and__", &argv[1], 1);
		}

		static Wg_Obj* object_ior(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__or__", &argv[1], 1);
		}

		static Wg_Obj* object_ixor(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__xor__", &argv[1], 1);
		}

		static Wg_Obj* object_ilshift(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__lshift__", &argv[1], 1);
		}

		static Wg_Obj* object_irshift(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__rshift__", &argv[1], 1);
		}

		static Wg_Obj* object_iter(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			return Wg_Call(context->builtins.defaultIter, argv, 1);
		}

		static Wg_Obj* object_reversed(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			return Wg_Call(context->builtins.defaultReverseIter, argv, 1);
		}

		static Wg_Obj* null_nonzero(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_NULL(0);
			return Wg_None(context);
		}

		static Wg_Obj* null_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_NULL(0);
			return Wg_NewString(context, "None");
		}

		static Wg_Obj* bool_int(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_BOOL(0);
			return Wg_NewInt(context, Wg_GetBool(argv[0]) ? 1 : 0);
		}

		static Wg_Obj* bool_float(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_BOOL(0);
			return Wg_NewFloat(context, Wg_GetBool(argv[0]) ? (Wg_float)1 : (Wg_float)0);
		}

		static Wg_Obj* bool_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_BOOL(0);
			return Wg_NewString(context, Wg_GetBool(argv[0]) ? "True" : "False");
		}

		static Wg_Obj* bool_eq(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_BOOL(0);
			return Wg_NewBool(context, Wg_IsBool(argv[1]) && Wg_GetBool(argv[0]) == Wg_GetBool(argv[1]));
		}

		static Wg_Obj* bool_hash(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_BOOL(0);
			Wg_int hash = (Wg_int)std::hash<bool>()(Wg_GetBool(argv[0]));
			return Wg_NewInt(context, hash);
		}

		static Wg_Obj* bool_abs(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_BOOL(0);
			return Wg_NewInt(context, Wg_GetBool(argv[0]) ? 1 : 0);
		}

		static Wg_Obj* int_nonzero(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_INT(0);
			return Wg_NewBool(context, Wg_GetInt(argv[0]) != 0);
		}

		static Wg_Obj* int_float(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_INT(0);
			return Wg_NewFloat(context, Wg_GetFloat(argv[0]));
		}

		static Wg_Obj* int_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_INT(0);
			return Wg_NewString(context, std::to_string(argv[0]->Get<Wg_int>()).c_str());
		}

		static Wg_Obj* int_eq(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(0);
			return Wg_NewBool(context, Wg_IsInt(argv[1]) && Wg_GetInt(argv[0]) == Wg_GetInt(argv[1]));
		}

		static Wg_Obj* int_lt(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(0);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return Wg_NewBool(context, Wg_GetFloat(argv[0]) < Wg_GetFloat(argv[1]));
		}

		static Wg_Obj* int_hash(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_INT(0);
			//Wg_int hash = (Wg_int)std::hash<Wg_int>()(Wg_GetInt(argv[0]));
			Wg_int hash = Wg_GetInt(argv[0]);
			return Wg_NewInt(context, hash);
		}

		static Wg_Obj* int_abs(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_INT(0);
			return Wg_NewInt(context, std::abs(Wg_GetInt(argv[0])));
		}

		static Wg_Obj* int_neg(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_INT(0);
			return Wg_NewInt(context, -Wg_GetInt(argv[0]));
		}

		static Wg_Obj* int_add(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(0);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			if (Wg_IsInt(argv[1])) {
				return Wg_NewInt(context, Wg_GetInt(argv[0]) + Wg_GetInt(argv[1]));
			} else {
				return Wg_NewFloat(context, Wg_GetFloat(argv[0]) + Wg_GetFloat(argv[1]));
			}
		}

		static Wg_Obj* int_sub(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(0);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			if (Wg_IsInt(argv[1])) {
				return Wg_NewInt(context, Wg_GetInt(argv[0]) - Wg_GetInt(argv[1]));
			} else {
				return Wg_NewFloat(context, Wg_GetFloat(argv[0]) - Wg_GetFloat(argv[1]));
			}
		}

		static Wg_Obj* int_mul(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(0);

			if (Wg_IsString(argv[1])) {
				Wg_int multiplier = Wg_GetInt(argv[0]);
				std::string s;
				for (Wg_int i = 0; i < multiplier; i++)
					s += Wg_GetString(argv[1]);
				return Wg_NewString(context, s.c_str());
			} else if (Wg_IsInt(argv[1])) {
				return Wg_NewInt(context, Wg_GetInt(argv[0]) * Wg_GetInt(argv[1]));
			} else if (Wg_IsIntOrFloat(argv[1])) {
				return Wg_NewFloat(context, Wg_GetFloat(argv[0]) * Wg_GetFloat(argv[1]));
			} else {
				WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
				return nullptr;
			}
		}

		static Wg_Obj* int_truediv(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(0);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);

			if (Wg_GetFloat(argv[1]) == 0) {
				Wg_RaiseException(context, WG_EXC_ZERODIVISIONERROR);
				return nullptr;
			}
			return Wg_NewFloat(context, Wg_GetFloat(argv[0]) / Wg_GetFloat(argv[1]));
		}

		static Wg_Obj* int_floordiv(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(0);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);

			if (Wg_GetFloat(argv[1]) == 0) {
				Wg_RaiseException(context, WG_EXC_ZERODIVISIONERROR);
				return nullptr;
			}

			if (Wg_IsInt(argv[1])) {
				return Wg_NewInt(context, (Wg_int)std::floor(Wg_GetFloat(argv[0]) / Wg_GetFloat(argv[1])));
			} else {
				return Wg_NewFloat(context, std::floor(Wg_GetFloat(argv[0]) / Wg_GetFloat(argv[1])));
			}
		}

		static Wg_Obj* int_mod(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(0);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);

			if (Wg_GetFloat(argv[1]) == 0) {
				Wg_RaiseException(context, WG_EXC_ZERODIVISIONERROR);
				return nullptr;
			}

			if (Wg_IsInt(argv[1])) {
				Wg_int mod = Wg_GetInt(argv[1]);
				Wg_int m = Wg_GetInt(argv[0]) % mod;
				if (m < 0)
					m += mod;
				return Wg_NewInt(context, m);
			} else {
				return Wg_NewFloat(context, std::fmod(Wg_GetFloat(argv[0]), Wg_GetFloat(argv[1])));
			}
		}

		static Wg_Obj* int_pow(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(0);
			
			if (Wg_IsInt(argv[1])) {
				return Wg_NewInt(context, (Wg_int)std::pow(Wg_GetFloat(argv[0]), Wg_GetFloat(argv[1])));
			} else {
				WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
				return Wg_NewFloat(context, std::pow(Wg_GetFloat(argv[0]), Wg_GetFloat(argv[1])));
			}
		}

		static Wg_Obj* int_and(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(0);
			WG_EXPECT_ARG_TYPE_INT(1);
			return Wg_NewInt(context, Wg_GetInt(argv[0]) & Wg_GetInt(argv[1]));
		}

		static Wg_Obj* int_or(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(0);
			WG_EXPECT_ARG_TYPE_INT(1);
			return Wg_NewInt(context, Wg_GetInt(argv[0]) | Wg_GetInt(argv[1]));
		}

		static Wg_Obj* int_xor(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(0);
			WG_EXPECT_ARG_TYPE_INT(1);
			return Wg_NewInt(context, Wg_GetInt(argv[0]) ^ Wg_GetInt(argv[1]));
		}

		static Wg_Obj* int_invert(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_INT(0);
			return Wg_NewInt(context, ~Wg_GetInt(argv[0]));
		}

		static Wg_Obj* int_lshift(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(0);
			WG_EXPECT_ARG_TYPE_INT(1);

			Wg_int shift = Wg_GetInt(argv[1]);
			if (shift < 0) {
				Wg_RaiseException(context, WG_EXC_VALUEERROR, "Shift cannot be negative");
				return nullptr;
			}
			shift = std::min(shift, (Wg_int)sizeof(Wg_int) * 8);
			return Wg_NewInt(context, Wg_GetInt(argv[0]) << shift);
		}

		static Wg_Obj* int_rshift(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(0);
			WG_EXPECT_ARG_TYPE_INT(1);

			Wg_int shift = Wg_GetInt(argv[1]);
			if (shift < 0) {
				Wg_RaiseException(context, WG_EXC_VALUEERROR, "Shift cannot be negative");
				return nullptr;
			}
			shift = std::min(shift, (Wg_int)sizeof(Wg_int) * 8);
			Wg_uint shifted = (Wg_uint)Wg_GetInt(argv[0]) >> shift;
			Wg_int i{};
			std::memcpy(&i, &shifted, sizeof(Wg_int));
			return Wg_NewInt(context, i);
		}

		static Wg_Obj* int_bit_length(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_INT(0);

			Wg_uint n = (Wg_uint)Wg_GetInt(argv[0]);
			return Wg_NewInt(context, (Wg_int)std::bit_width(n));
		}

		static Wg_Obj* int_bit_count(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_INT(0);

			Wg_uint n = (Wg_uint)Wg_GetInt(argv[0]);
			return Wg_NewInt(context, (Wg_int)std::popcount(n));
		}

		static Wg_Obj* float_nonzero(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			return Wg_NewBool(context, Wg_GetFloat(argv[0]) != 0);
		}

		static Wg_Obj* float_int(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			return Wg_NewInt(context, (Wg_int)Wg_GetFloat(argv[0]));
		}

		static Wg_Obj* float_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_FLOAT(0);
			std::string s = std::to_string(argv[0]->Get<Wg_float>());
			s.erase(s.find_last_not_of('0') + 1, std::string::npos);
			if (s.ends_with('.'))
				s.push_back('0');
			return Wg_NewString(context, s.c_str());
		}

		static Wg_Obj* float_eq(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			return Wg_NewBool(context, Wg_IsIntOrFloat(argv[1]) && Wg_GetFloat(argv[0]) == Wg_GetFloat(argv[1]));
		}

		static Wg_Obj* float_lt(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return Wg_NewBool(context, Wg_GetFloat(argv[0]) < Wg_GetFloat(argv[1]));
		}

		static Wg_Obj* float_hash(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_FLOAT(0);
			Wg_int hash = (Wg_int)std::hash<Wg_float>()(Wg_GetFloat(argv[0]));
			return Wg_NewInt(context, hash);
		}

		static Wg_Obj* float_abs(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_FLOAT(0);
			return Wg_NewFloat(context, std::abs(Wg_GetFloat(argv[0])));
		}

		static Wg_Obj* float_neg(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			return Wg_NewFloat(context, -Wg_GetFloat(argv[0]));
		}

		static Wg_Obj* float_add(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return Wg_NewFloat(context, Wg_GetFloat(argv[0]) + Wg_GetFloat(argv[1]));
		}

		static Wg_Obj* float_sub(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return Wg_NewFloat(context, Wg_GetFloat(argv[0]) - Wg_GetFloat(argv[1]));
		}

		static Wg_Obj* float_mul(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return Wg_NewFloat(context, Wg_GetFloat(argv[0]) * Wg_GetFloat(argv[1]));
		}

		static Wg_Obj* float_truediv(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return Wg_NewFloat(context, Wg_GetFloat(argv[0]) / Wg_GetFloat(argv[1]));
		}

		static Wg_Obj* float_floordiv(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return Wg_NewFloat(context, std::floor(Wg_GetFloat(argv[0]) / Wg_GetFloat(argv[1])));
		}

		static Wg_Obj* float_mod(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return Wg_NewFloat(context, std::fmod(Wg_GetFloat(argv[0]), Wg_GetFloat(argv[1])));
		}

		static Wg_Obj* float_pow(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return Wg_NewFloat(context, std::pow(Wg_GetFloat(argv[0]), Wg_GetFloat(argv[1])));
		}

		static Wg_Obj* float_is_integer(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_FLOAT(0);

			Wg_float f = Wg_GetFloat(argv[0]);
			return Wg_NewBool(context, std::floor(f) == f);
		}

		static Wg_Obj* str_nonzero(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_STRING(0);
			std::string s = Wg_GetString(argv[0]);
			return Wg_NewBool(context, !s.empty());
		}

		static Wg_Obj* str_int(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 2);
			WG_EXPECT_ARG_TYPE_STRING(0);

			constexpr std::string_view DIGITS = "0123456789abcdefghijklmnopqrstuvwxyz";

			auto isDigit = [&](char c, int base = 10) {
				auto sub = DIGITS.substr(0, base);
				return sub.find(std::tolower(c)) != std::string_view::npos;
			};

			auto digitValueOf = [](char c, int base) {
				return DIGITS.substr(0, base).find(std::tolower(c));
			};

			std::string s = Wg_GetString(argv[0]);
			const char* p = s.c_str();

			std::optional<int> expectedBase;
			if (argc == 2) {
				expectedBase = (int)Wg_GetInt(argv[1]);
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
						const char* message{};
						switch (base) {
						case 2: message = "Invalid binary string"; break;
						case 8: message = "Invalid octal string"; break;
						case 16: message = "Invalid hexadecimal string"; break;
						default: WG_UNREACHABLE();
						}
						Wg_RaiseException(context, WG_EXC_VALUEERROR, message);
						return nullptr;
					}
				}
			}

			uintmax_t value = 0;
			for (; *p && isDigit(*p, base); ++p) {
				value = (base * value) + digitValueOf(*p, base);
			}

			if (value > std::numeric_limits<Wg_uint>::max()) {
				Wg_RaiseException(context, WG_EXC_OVERFLOWERROR, "Integer string is too large");
				return nullptr;
			}

			if (*p) {
				Wg_RaiseException(context, WG_EXC_VALUEERROR, "Invalid integer string");
				return nullptr;
			}

			return Wg_NewInt(context, (Wg_int)value);
		}

		static Wg_Obj* str_float(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_STRING(0);

			auto isDigit = [](char c, int base = 10) {
				switch (base) {
				case 2: return c >= '0' && c <= '1';
				case 8: return c >= '0' && c <= '7';
				case 10: return c >= '0' && c <= '9';
				case 16: return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
				default: WG_UNREACHABLE();
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
					WG_UNREACHABLE();
				}
			};

			std::string s = Wg_GetString(argv[0]);
			const char* p = s.c_str();

			if (s == "inf") {
				return Wg_NewFloat(context, std::numeric_limits<Wg_float>::infinity());
			} else if (s == "-inf") {
				return Wg_NewFloat(context, -std::numeric_limits<Wg_float>::infinity());
			} else if (s == "nan") {
				return Wg_NewFloat(context, std::numeric_limits<Wg_float>::quiet_NaN());
			}

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
					const char* message{};
					switch (base) {
					case 2: message = "Invalid binary string"; break;
					case 8: message = "Invalid octal string"; break;
					case 16: message = "Invalid hexadecimal string"; break;
					default: WG_UNREACHABLE();
					}
					Wg_RaiseException(context, WG_EXC_VALUEERROR, message);
					return nullptr;
				}
			}

			uintmax_t value = 0;
			for (; *p && isDigit(*p, base); ++p) {
				value = (base * value) + digitValueOf(*p, base);
			}

			Wg_float fvalue = (Wg_float)value;
			if (*p == '.') {
				++p;
				for (int i = 1; *p && isDigit(*p, base); ++p, ++i) {
					fvalue += digitValueOf(*p, base) * std::pow((Wg_float)base, (Wg_float)-i);
				}
			}

			if (*p) {
				Wg_RaiseException(context, WG_EXC_VALUEERROR, "Invalid float string");
				return nullptr;
			}

			return Wg_NewFloat(context, fvalue);
		}

		static Wg_Obj* str_repr(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_STRING(0);
			
			std::string s = "'";
			for (const char* p = Wg_GetString(argv[0]); *p; ++p) {
				if (*p == '\\') {
					s += "\\\\";
				} else if (*p == '\'') {
					s += "\\'";
				} else if (*p == '\n') {
					s += "\\n";
				} else if (*p == '\r') {
					s += "\\r";
				} else if (*p == '\t') {
					s += "\\t";
				} else if (*p == '\b') {
					s += "\\b";
				} else if (*p == '\f') {
					s += "\\f";
				} else if (*p >= 32 && *p <= 126) {
					s.push_back(*p);
				} else {
					s += "\\x";
					s.push_back("0123456789abcdef"[(*p >> 4) & 0xF]);
					s.push_back("0123456789abcdef"[*p & 0xF]);
				}
			}
			s.push_back('\'');
			
			return Wg_NewString(context, s.c_str());
		}

		static Wg_Obj* str_len(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_STRING(0);
			return Wg_NewInt(context, (Wg_int)argv[0]->Get<std::string>().size());
		}

		static Wg_Obj* str_eq(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_STRING(0);
			return Wg_NewBool(context, Wg_IsString(argv[1]) && std::strcmp(Wg_GetString(argv[0]), Wg_GetString(argv[1])) == 0);
		}

		static Wg_Obj* str_lt(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_STRING(0);
			WG_EXPECT_ARG_TYPE_STRING(1);
			return Wg_NewBool(context, std::strcmp(Wg_GetString(argv[0]), Wg_GetString(argv[1])) < 0);
		}

		static Wg_Obj* str_hash(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_STRING(0);
			Wg_int hash = (Wg_int)std::hash<std::string_view>()(Wg_GetString(argv[0]));
			return Wg_NewInt(context, hash);
		}

		static Wg_Obj* str_add(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_STRING(0);
			WG_EXPECT_ARG_TYPE_STRING(1);
			std::string s = Wg_GetString(argv[0]);
			s += Wg_GetString(argv[1]);
			return Wg_NewString(context, s.c_str());
		}

		static Wg_Obj* str_mul(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_STRING(0);
			WG_EXPECT_ARG_TYPE_INT(1);
			Wg_int multiplier = Wg_GetInt(argv[1]);
			std::string_view arg = Wg_GetString(argv[0]);
			std::string s;
			s.reserve(arg.size() * (size_t)multiplier);
			for (Wg_int i = 0; i < multiplier; i++)
				s += arg;
			return Wg_NewString(context, s.c_str());
		}

		static Wg_Obj* str_contains(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_STRING(0);
			WG_EXPECT_ARG_TYPE_STRING(1);
			return Wg_NewBool(context, std::strstr(Wg_GetString(argv[0]), Wg_GetString(argv[1])));
		}

		static Wg_Obj* str_getitem(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_STRING(0);

			if (Wg_IsInstance(argv[1], &context->builtins.slice, 1)) {
				Wg_int start, stop, step;
				if (!AbsSlice(argv[0], argv[1], start, stop, step))
					return nullptr;

				std::string_view s = Wg_GetString(argv[0]);
				std::string sliced;
				bool success = IterateRange(start, stop, step, [&](Wg_int i) {
					if (i >= 0 && i < (Wg_int)s.size())
						sliced.push_back(s[i]);
					return true;
					});

				if (!success)
					return nullptr;

				return Wg_NewString(context, sliced.c_str());
			}

			Wg_Obj* idx = Wg_UnaryOp(WG_UOP_INDEX, argv[1]);
			if (idx == nullptr)
				return nullptr;
			
			if (Wg_IsInt(idx)) {
				Wg_int index;
				if (!AbsIndex(argv[0], idx, index))
					return nullptr;

				std::string_view s = Wg_GetString(argv[0]);
				if (index < 0 || index >= (Wg_int)s.size()) {
					Wg_RaiseException(context, WG_EXC_INDEXERROR);
					return nullptr;
				}

				char buf[2] = { s[index], '\0' };
				return Wg_NewString(context, buf);
			}
			
			Wg_RaiseArgumentTypeError(context, 1, "int or slice");
			return nullptr;
		}

		static Wg_Obj* str_capitalize(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_STRING(0);
			std::string s = Wg_GetString(argv[0]);
			if (!s.empty())
				s[0] = (char)std::toupper(s[0]);
			return Wg_NewString(context, s.c_str());
		}

		static Wg_Obj* str_lower(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_STRING(0);

			std::string s = Wg_GetString(argv[0]);
			std::transform(s.begin(), s.end(), s.begin(), std::tolower);
			return Wg_NewString(context, s.c_str());
		}

		static Wg_Obj* str_upper(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_STRING(0);

			std::string s = Wg_GetString(argv[0]);
			std::transform(s.begin(), s.end(), s.begin(), std::toupper);
			return Wg_NewString(context, s.c_str());
		}

		static Wg_Obj* str_casefold(Wg_Context* context, Wg_Obj** argv, int argc) {
			return str_lower(context, argv, argc);
		}

		static Wg_Obj* str_center(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(2, 3);
			WG_EXPECT_ARG_TYPE_STRING(0);
			WG_EXPECT_ARG_TYPE_INT(1);
			if (argc >= 3) WG_EXPECT_ARG_TYPE_STRING(2);
			
			const char* fill = argc == 3 ? Wg_GetString(argv[2]) : " ";
			if (std::strlen(fill) != 1) {
				Wg_RaiseException(context, WG_EXC_TYPEERROR, "The fill character must be exactly one character long");
				return nullptr;
			}

			std::string s = Wg_GetString(argv[0]);
			Wg_int desiredLen = Wg_GetInt(argv[1]);
			while (true) {
				if ((Wg_int)s.size() >= desiredLen)
					break;
				s.push_back(fill[0]);
				if ((Wg_int)s.size() >= desiredLen)
					break;
				s.insert(s.begin(), fill[0]);
			}

			return Wg_NewString(context, s.c_str());
		}

		static Wg_Obj* str_count(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_STRING(0);
			WG_EXPECT_ARG_TYPE_STRING(1);
			
			std::string_view s = Wg_GetString(argv[0]);
			std::string_view search = Wg_GetString(argv[1]);			
			Wg_int count = 0;
			size_t pos = 0;
			while ((pos = s.find(search, pos)) != std::string_view::npos) {
				count++;
				pos += search.size();
			}

			return Wg_NewInt(context, count);
		}

		static Wg_Obj* str_format(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_AT_LEAST(1);
			WG_EXPECT_ARG_TYPE_STRING(0);
			
			const char* fmt = Wg_GetString(argv[0]);
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
						Wg_RaiseException(context, WG_EXC_VALUEERROR, "Invalid format string");
						return nullptr;
					}
				}

				if (useAutoIndexing) {
					if (mode == Mode::Manual) {
						Wg_RaiseException(
							context,
							WG_EXC_VALUEERROR,
							"Cannot switch from manual field numbering to automatic field specification"
						);
						return nullptr;
					}
					mode = Mode::Auto;
					index = autoIndex;
					autoIndex++;
				} else {
					if (mode == Mode::Auto) {
						Wg_RaiseException(
							context,
							WG_EXC_VALUEERROR,
							"Cannot switch from automatic field numbering to manual field specification"
						);
						return nullptr;
					}
					mode = Mode::Manual;
				}

				if ((int)index >= argc - 1) {
					Wg_RaiseException(context, WG_EXC_INDEXERROR);
					return nullptr;
				}

				Wg_Obj* item = Wg_UnaryOp(WG_UOP_STR, argv[index + 1]);
				if (item == nullptr)
					return nullptr;
				s += Wg_GetString(item);
			}

			return Wg_NewString(context, s.c_str());
		}

		static Wg_Obj* str_startswith(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_STRING(0);
			WG_EXPECT_ARG_TYPE_STRING(1);

			std::string_view s = Wg_GetString(argv[0]);
			std::string_view end = Wg_GetString(argv[1]);
			return Wg_NewBool(context, s.starts_with(end));
		}

		static Wg_Obj* str_endswith(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_STRING(0);
			WG_EXPECT_ARG_TYPE_STRING(1);

			std::string_view s = Wg_GetString(argv[0]);
			std::string_view end = Wg_GetString(argv[1]);
			return Wg_NewBool(context, s.ends_with(end));
		}

		template <bool reverse>
		static Wg_Obj* str_findx(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(2, 4);
			WG_EXPECT_ARG_TYPE_STRING(0);
			WG_EXPECT_ARG_TYPE_STRING(1);
			
			Wg_int start = 0;
			std::optional<Wg_int> size;
			if (argc >= 3) {
				WG_EXPECT_ARG_TYPE_INT(2);
				if (!AbsIndex(argv[0], argv[2], start, size))
					return nullptr;
			}

			Wg_int end = 0;
			if (argc >= 4) {
				WG_EXPECT_ARG_TYPE_INT(3);
				if (!AbsIndex(argv[0], argv[3], end, size))
					return nullptr;
			} else {
				Wg_Obj* len = Wg_UnaryOp(WG_UOP_LEN, argv[0]);
				if (len == nullptr)
					return nullptr;
				end = (size_t)Wg_GetInt(len);
			}
			
			std::string_view s = Wg_GetString(argv[0]);
			std::string_view find = Wg_GetString(argv[1]);
			
			Wg_int substrSize = end - start;
			size_t location;
			if (substrSize < 0) {
				location = std::string_view::npos;
			} else {
				start = std::clamp(start, (Wg_int)0, (Wg_int)s.size());
				if (reverse) {
					location = s.substr(start, (size_t)substrSize).rfind(find);
				} else {
					location = s.substr(start, (size_t)substrSize).find(find);
				}
			}
			
			if (location == std::string_view::npos) {
				return Wg_NewInt(context, -1);
			} else {
				return Wg_NewInt(context, (Wg_int)location);
			}
		}

		template <bool reverse>
		static Wg_Obj* str_indexx(Wg_Context* context, Wg_Obj** argv, int argc) {
			Wg_Obj* location = str_findx<reverse>(context, argv, argc);
			if (location == nullptr)
				return nullptr;
			
			if (Wg_GetInt(location) == -1) {
				Wg_RaiseException(context, WG_EXC_VALUEERROR, "substring not found");
				return nullptr;
			} else {
				return location;
			}
		}

		static Wg_Obj* str_find(Wg_Context* context, Wg_Obj** argv, int argc) {
			return str_findx<false>(context, argv, argc);
		}

		static Wg_Obj* str_index(Wg_Context* context, Wg_Obj** argv, int argc) {
			return str_indexx<false>(context, argv, argc);
		}

		static Wg_Obj* str_rfind(Wg_Context* context, Wg_Obj** argv, int argc) {
			return str_findx<true>(context, argv, argc);
		}

		static Wg_Obj* str_rindex(Wg_Context* context, Wg_Obj** argv, int argc) {
			return str_indexx<true>(context, argv, argc);
		}

		template <auto F>
		static Wg_Obj* str_isx(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_STRING(0);

			std::string_view s = Wg_GetString(argv[0]);
			return Wg_NewBool(context, std::all_of(s.begin(), s.end(), F));
		}

		static Wg_Obj* str_isalnum(Wg_Context* context, Wg_Obj** argv, int argc) {
			constexpr auto f = [](char c) { return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9'); };
			return str_isx<f>(context, argv, argc);
		}

		static Wg_Obj* str_isalpha(Wg_Context* context, Wg_Obj** argv, int argc) {
			constexpr auto f = [](char c) { return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z'); };
			return str_isx<f>(context, argv, argc);
		}

		static Wg_Obj* str_isascii(Wg_Context* context, Wg_Obj** argv, int argc) {
			constexpr auto f = [](char c) { return c < 128; };
			return str_isx<f>(context, argv, argc);
		}

		static Wg_Obj* str_isdigit(Wg_Context* context, Wg_Obj** argv, int argc) {
			constexpr auto f = [](char c) { return '0' <= c && c <= '9'; };
			return str_isx<f>(context, argv, argc);
		}

		static Wg_Obj* str_isdecimal(Wg_Context* context, Wg_Obj** argv, int argc) {
			return str_isdigit(context, argv, argc);
		}

		static Wg_Obj* str_isnumeric(Wg_Context* context, Wg_Obj** argv, int argc) {
			return str_isdigit(context, argv, argc);
		}

		static Wg_Obj* str_isprintable(Wg_Context* context, Wg_Obj** argv, int argc) {
			constexpr auto f = [](char c) { return c >= 32 && c <= 127; };
			return str_isx<f>(context, argv, argc);
		}

		static Wg_Obj* str_isspace(Wg_Context* context, Wg_Obj** argv, int argc) {
			return str_isx<IsSpace>(context, argv, argc);
		}

		static Wg_Obj* str_isupper(Wg_Context* context, Wg_Obj** argv, int argc) {
			constexpr auto f = [](char c) { return !('a' <= c && c <= 'z'); };
			return str_isx<f>(context, argv, argc);
		}

		static Wg_Obj* str_islower(Wg_Context* context, Wg_Obj** argv, int argc) {
			constexpr auto f = [](char c) { return !('A' <= c && c <= 'Z'); };
			return str_isx<f>(context, argv, argc);
		}

		static Wg_Obj* str_isidentifier(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_STRING(0);

			std::string_view s = Wg_GetString(argv[0]);
			constexpr auto f = [](char c) { return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9') || c == '_'; };
			bool allAlphaNum = std::all_of(s.begin(), s.end(), f);
			return Wg_NewBool(context, allAlphaNum && (s.empty() || s[0] < '0' || s[0] > '9'));
		}

		static Wg_Obj* str_join(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_STRING(0);

			struct State {
				std::string_view sep;
				std::string s;
			} state = { Wg_GetString(argv[0]) };

			bool success = Wg_Iterate(argv[1], &state, [](Wg_Obj* obj, void* ud) {
				State& state = *(State*)ud;
				Wg_Context* context = obj->context;

				if (!Wg_IsString(obj)) {
					Wg_RaiseException(context, WG_EXC_TYPEERROR, "sequence item must be a string");
					return false;
				}
				
				state.s += Wg_GetString(obj);
				state.s += state.sep;
				return true;
			});

			if (!success)
				return nullptr;

			if (!state.s.empty())
				state.s.erase(state.s.end() - state.sep.size(), state.s.end());

			return Wg_NewString(context, state.s.c_str());
		}

		static Wg_Obj* str_replace(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(3, 4);
			WG_EXPECT_ARG_TYPE_STRING(0);
			WG_EXPECT_ARG_TYPE_STRING(1);
			WG_EXPECT_ARG_TYPE_STRING(2);

			Wg_int count = std::numeric_limits<Wg_int>::max();
			if (argc == 4) {
				WG_EXPECT_ARG_TYPE_INT(3);
				count = Wg_GetInt(argv[3]);
			}

			std::string s = Wg_GetString(argv[0]);
			std::string_view find = Wg_GetString(argv[1]);
			std::string_view repl = Wg_GetString(argv[2]);
			StringReplace(s, find, repl, count);
			return Wg_NewString(context, s.c_str());
		}

		template <bool left, bool zfill = false>
		static Wg_Obj* str_just(Wg_Context* context, Wg_Obj** argv, int argc) {
			if constexpr (zfill) {
				WG_EXPECT_ARG_COUNT(2);
			} else {
				WG_EXPECT_ARG_COUNT_BETWEEN(2, 3);
			}
			WG_EXPECT_ARG_TYPE_STRING(0);
			WG_EXPECT_ARG_TYPE_INT(1);

			char fill = ' ';
			if constexpr (!zfill) {
				if (argc == 3) {
					WG_EXPECT_ARG_TYPE_STRING(0);
					std::string_view fillStr = Wg_GetString(argv[2]);
					if (fillStr.size() != 1) {
						Wg_RaiseException(context, WG_EXC_TYPEERROR, "The fill character must be exactly one character long");
						return nullptr;
					}
					fill = fillStr[0];
				}
			} else {
				fill = '0';
			}

			std::string s = Wg_GetString(argv[0]);

			Wg_int len = Wg_GetInt(argv[1]);
			if (len < (Wg_int)s.size())
				return argv[0];

			if (left) {
				s += std::string((size_t)len - s.size(), fill);
			} else {
				s = s + std::string((size_t)len - s.size(), fill);
			}
			return Wg_NewString(context, s.c_str());
		}

		static Wg_Obj* str_ljust(Wg_Context* context, Wg_Obj** argv, int argc) {
			return str_just<true>(context, argv, argc);
		}

		static Wg_Obj* str_rjust(Wg_Context* context, Wg_Obj** argv, int argc) {
			return str_just<false>(context, argv, argc);
		}

		static Wg_Obj* str_zfill(Wg_Context* context, Wg_Obj** argv, int argc) {
			return str_just<true, true>(context, argv, argc);
		}

		static Wg_Obj* str_lstrip(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 2);
			WG_EXPECT_ARG_TYPE_STRING(0);

			std::string_view chars = " ";
			if (argc == 2 && !Wg_IsNone(argv[1])) {
				WG_EXPECT_ARG_TYPE_STRING(1);
				chars = Wg_GetString(argv[1]);
			}

			std::string_view s = Wg_GetString(argv[0]);
			size_t pos = s.find_first_not_of(chars);
			if (pos == std::string::npos)
				return Wg_NewString(context);
			return Wg_NewString(context, s.data() + pos);
		}

		static Wg_Obj* str_rstrip(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 2);
			WG_EXPECT_ARG_TYPE_STRING(0);

			std::string_view chars = " ";
			if (argc == 2 && !Wg_IsNone(argv[1])) {
				WG_EXPECT_ARG_TYPE_STRING(1);
				chars = Wg_GetString(argv[1]);
			}

			std::string s = Wg_GetString(argv[0]);
			size_t pos = s.find_last_not_of(chars);
			if (pos == std::string::npos)
				return Wg_NewString(context);
			s.erase(s.begin() + pos + 1, s.end());
			return Wg_NewString(context, s.c_str());
		}

		static Wg_Obj* str_strip(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 2);
			WG_EXPECT_ARG_TYPE_STRING(0);

			std::string_view chars = " ";
			if (argc == 2 && !Wg_IsNone(argv[1])) {
				WG_EXPECT_ARG_TYPE_STRING(1);
				chars = Wg_GetString(argv[1]);
			}

			std::string s = Wg_GetString(argv[0]);
			size_t pos = s.find_last_not_of(chars);
			if (pos == std::string::npos)
				return Wg_NewString(context);
			s.erase(s.begin() + pos + 1, s.end());

			pos = s.find_first_not_of(chars);
			if (pos == std::string::npos)
				return Wg_NewString(context);
			return Wg_NewString(context, s.data() + pos);
		}

		static Wg_Obj* str_split(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 3);
			WG_EXPECT_ARG_TYPE_STRING(0);

			Wg_int maxSplit = -1;
			if (argc == 3) {
				WG_EXPECT_ARG_TYPE_INT(2);
				maxSplit = Wg_GetInt(argv[2]);
			}
			if (maxSplit == -1)
				maxSplit = std::numeric_limits<Wg_int>::max();

			std::vector<std::string> strings;
			if (argc >= 2) {
				WG_EXPECT_ARG_TYPE_STRING(1);
				strings = StringSplit(Wg_GetString(argv[0]), Wg_GetString(argv[1]), maxSplit);
			} else {
				strings = StringSplitChar(Wg_GetString(argv[0]), " \t\n\r\v\f", maxSplit);
			}

			Wg_Obj* li = Wg_NewList(context);
			if (li == nullptr)
				return nullptr;
			Wg_ObjRef ref(li);

			for (const auto& s : strings) {
				Wg_Obj* str = Wg_NewString(context, s.c_str());
				if (str == nullptr)
					return nullptr;
				li->Get<std::vector<Wg_Obj*>>().push_back(str);
			}
			return li;
		}

		static Wg_Obj* str_splitlines(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 2);
			WG_EXPECT_ARG_TYPE_STRING(0);

			bool keepLineBreaks = false;
			if (argc == 2) {
				WG_EXPECT_ARG_TYPE_BOOL(1);
				keepLineBreaks = Wg_GetBool(argv[1]);
			}

			std::vector<std::string> strings = StringSplitLines(Wg_GetString(argv[0]), keepLineBreaks);

			Wg_Obj* li = Wg_NewList(context);
			if (li == nullptr)
				return nullptr;
			Wg_ObjRef ref(li);

			for (const auto& s : strings) {
				Wg_Obj* str = Wg_NewString(context, s.c_str());
				if (str == nullptr)
					return nullptr;
				li->Get<std::vector<Wg_Obj*>>().push_back(str);
			}
			return li;
		}

		template <Collection collection>
		static Wg_Obj* collection_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			constexpr bool isTuple = collection == Collection::Tuple;
			WG_EXPECT_ARG_COUNT(1);
			if constexpr (isTuple) {
				WG_EXPECT_ARG_TYPE_TUPLE(0);
			} else {
				WG_EXPECT_ARG_TYPE_LIST(0);
			}

			auto it = std::find(context->reprStack.rbegin(), context->reprStack.rend(), argv[0]);
			if (it != context->reprStack.rend()) {
				return Wg_NewString(context, isTuple ? "(...)" : "[...]");
			} else {
				context->reprStack.push_back(argv[0]);
				const auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();
				std::string s(1, isTuple ? '(' : '[');
				for (Wg_Obj* child : buf) {
					Wg_Obj* v = Wg_UnaryOp(WG_UOP_REPR, child);
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
				return Wg_NewString(context, s.c_str());
			}
		}

		template <Collection collection>
		static Wg_Obj* collection_nonzero(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			if constexpr (collection == Collection::List) {
				WG_EXPECT_ARG_TYPE_LIST(0);
			} else {
				WG_EXPECT_ARG_TYPE_TUPLE(0);
			}

			return Wg_NewBool(context, !argv[0]->Get<std::vector<Wg_Obj*>>().empty());
		}

		template <Collection collection>
		static Wg_Obj* collection_lt(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			if constexpr (collection == Collection::List) {
				WG_EXPECT_ARG_TYPE_LIST(0);
				WG_EXPECT_ARG_TYPE_LIST(1);
			} else {
				WG_EXPECT_ARG_TYPE_TUPLE(0);
				WG_EXPECT_ARG_TYPE_TUPLE(1);
			}

			auto& buf1 = argv[0]->Get<std::vector<Wg_Obj*>>();
			auto& buf2 = argv[1]->Get<std::vector<Wg_Obj*>>();

			size_t minSize = buf1.size() < buf2.size() ? buf1.size() : buf2.size();

			for (size_t i = 0; i < minSize; i++) {
				Wg_Obj* lt = Wg_BinaryOp(WG_BOP_LT, buf1[i], buf2[i]);
				if (lt == nullptr)
					return nullptr;

				if (Wg_GetBool(lt))
					return lt;

				Wg_Obj* gt = Wg_BinaryOp(WG_BOP_LT, buf1[i], buf2[i]);
				if (gt == nullptr)
					return nullptr;

				if (Wg_GetBool(gt))
					return Wg_NewBool(context, false);
			}

			return Wg_NewBool(context, buf1.size() < buf2.size());
		}

		template <Collection collection>
		static Wg_Obj* collection_eq(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			if constexpr (collection == Collection::List) {
				WG_EXPECT_ARG_TYPE_LIST(0);
				if (!Wg_IsInstance(argv[1], &context->builtins.list, 1))
					return Wg_NewBool(context, false);
			} else {
				WG_EXPECT_ARG_TYPE_TUPLE(0);
				if (!Wg_IsInstance(argv[1], &context->builtins.tuple, 1))
					return Wg_NewBool(context, false);
			}

			auto& buf1 = argv[0]->Get<std::vector<Wg_Obj*>>();
			auto& buf2 = argv[1]->Get<std::vector<Wg_Obj*>>();

			if (buf1.size() != buf2.size())
				return Wg_NewBool(context, false);

			for (size_t i = 0; i < buf1.size(); i++) {
				if (Wg_Obj* eq = Wg_BinaryOp(WG_BOP_EQ, buf1[i], buf2[i])) {
					if (!Wg_GetBool(eq))
						return eq;
				} else {
					return nullptr;
				}
			}

			return Wg_NewBool(context, true);
		}

		template <Collection collection>
		static Wg_Obj* collection_contains(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			if constexpr (collection == Collection::List) {
				WG_EXPECT_ARG_TYPE_LIST(0);
			} else {
				WG_EXPECT_ARG_TYPE_TUPLE(0);
			}

			auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();
			for (size_t i = 0; i < buf.size(); i++) {
				if (Wg_Obj* eq = Wg_BinaryOp(WG_BOP_EQ, buf[i], argv[1])) {
					if (Wg_GetBool(eq))
						return eq;
				} else {
					return nullptr;
				}
			}

			return Wg_NewBool(context, false);
		}

		template <Collection collection>
		static Wg_Obj* collection_len(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			if constexpr (collection == Collection::List) {
				WG_EXPECT_ARG_TYPE_LIST(0);
			} else {
				WG_EXPECT_ARG_TYPE_TUPLE(0);
			}

			return Wg_NewInt(context, (Wg_int)argv[0]->Get<std::vector<Wg_Obj*>>().size());
		}

		template <Collection collection>
		static Wg_Obj* collection_count(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			if constexpr (collection == Collection::List) {
				WG_EXPECT_ARG_TYPE_LIST(0);
			} else {
				WG_EXPECT_ARG_TYPE_TUPLE(0);
			}

			auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();
			Wg_int count = 0;
			for (size_t i = 0; i < buf.size(); i++) {
				Wg_Obj* eq = Wg_BinaryOp(WG_BOP_EQ, argv[1], buf[i]);
				if (eq == nullptr)
					return nullptr;
				if (Wg_GetBool(eq))
					count++;
			}

			return Wg_NewInt(context, count);
		}

		template <Collection collection>
		static Wg_Obj* collection_index(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			if constexpr (collection == Collection::List) {
				WG_EXPECT_ARG_TYPE_LIST(0);
			} else {
				WG_EXPECT_ARG_TYPE_TUPLE(0);
			}

			auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();
			for (size_t i = 0; i < buf.size(); i++) {
				Wg_Obj* eq = Wg_BinaryOp(WG_BOP_EQ, argv[1], buf[i]);
				if (eq == nullptr)
					return nullptr;
				if (Wg_GetBool(eq))
					return Wg_NewInt(context, (Wg_int)i);
			}

			Wg_RaiseException(context, WG_EXC_VALUEERROR, "Value was not found");
			return nullptr;
		}

		template <Collection collection>
		static Wg_Obj* collection_getitem(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			if constexpr (collection == Collection::List) {
				WG_EXPECT_ARG_TYPE_LIST(0);
			} else {
				WG_EXPECT_ARG_TYPE_TUPLE(0);
			}

			if (Wg_IsInstance(argv[1], &context->builtins.slice, 1)) {
				Wg_int start, stop, step;
				if (!AbsSlice(argv[0], argv[1], start, stop, step))
					return nullptr;

				auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();
				std::vector<Wg_Obj*> sliced;
				bool success = IterateRange(start, stop, step, [&](Wg_int i) {
					if (i >= 0 && i < (Wg_int)buf.size())
						sliced.push_back(buf[i]);
					return true;
					});

				if (!success)
					return nullptr;

				if constexpr (collection == Collection::List) {
					return Wg_NewList(context, sliced.data(), (int)sliced.size());
				} else {
					return Wg_NewTuple(context, sliced.data(), (int)sliced.size());
				}
			}

			Wg_Obj* idx = Wg_UnaryOp(WG_UOP_INDEX, argv[1]);
			if (idx == nullptr)
				return nullptr;

			if (Wg_IsInt(idx)) {
				Wg_int index;
				if (!AbsIndex(argv[0], idx, index))
					return nullptr;

				auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();
				if (index < 0 || index >= (Wg_int)buf.size()) {
					Wg_RaiseException(context, WG_EXC_INDEXERROR);
					return nullptr;
				}

				return buf[index];
			}

			Wg_RaiseArgumentTypeError(context, 1, "int or slice");
			return nullptr;
		}

		static Wg_Obj* list_setitem(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(3);
			WG_EXPECT_ARG_TYPE_LIST(0);
			WG_EXPECT_ARG_TYPE_INT(1);
			
			Wg_int index;
			if (!AbsIndex(argv[0], argv[1], index))
				return nullptr;

			auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();
			if (index < 0 || index >= (Wg_int)buf.size()) {
				Wg_RaiseException(context, WG_EXC_INDEXERROR);
				return nullptr;
			}

			buf[index] = argv[2];
			return Wg_None(context);
		}
		
		static Wg_Obj* list_append(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_LIST(0);

			argv[0]->Get<std::vector<Wg_Obj*>>().push_back(argv[1]);
			return Wg_None(context);
		}

		static Wg_Obj* list_insert(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(3);
			WG_EXPECT_ARG_TYPE_LIST(0);
			WG_EXPECT_ARG_TYPE_INT(1);

			Wg_int index;
			if (!AbsIndex(argv[0], argv[1], index))
				return nullptr;

			auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();			
			index = std::clamp(index, (Wg_int)0, (Wg_int)buf.size() + 1);
			buf.insert(buf.begin() + index, argv[2]);
			return Wg_None(context);
		}

		static Wg_Obj* list_pop(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 2);
			WG_EXPECT_ARG_TYPE_LIST(0);

			auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();
			Wg_int index = (Wg_int)buf.size() - 1;
			if (argc == 2) {
				WG_EXPECT_ARG_TYPE_INT(1);
				if (!AbsIndex(argv[0], argv[1], index))
					return nullptr;
			}

			if (index < 0 || index >= (Wg_int)buf.size()) {
				Wg_RaiseException(context, WG_EXC_INDEXERROR);
				return nullptr;
			}
			
			Wg_Obj* popped = buf[index];
			buf.erase(buf.begin() + index);
			return popped;
		}

		static Wg_Obj* list_remove(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_LIST(0);

			auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();
			for (size_t i = 0; i < buf.size(); i++) {
				Wg_Obj* eq = Wg_BinaryOp(WG_BOP_EQ, argv[1], buf[i]);
				if (eq == nullptr)
					return nullptr;
				
				if (Wg_GetBool(eq)) {
					if (i < buf.size())
						buf.erase(buf.begin() + i);
					return Wg_None(context);
				}
			}

			Wg_RaiseException(context, WG_EXC_VALUEERROR, "Value was not found");
			return nullptr;
		}

		static Wg_Obj* list_clear(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_LIST(0);

			argv[0]->Get<std::vector<Wg_Obj*>>().clear();
			return Wg_None(context);
		}

		static Wg_Obj* list_copy(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_LIST(0);

			auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();
			return Wg_NewList(context, buf.data(), !buf.size());
		}

		static Wg_Obj* list_extend(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_LIST(0);

			auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();

			if (argv[0] == argv[1]) {
				// Double the list instead of going into an infinite loop
				buf.insert(buf.end(), buf.begin(), buf.end());
			} else {
				bool success = Wg_Iterate(argv[1], &buf, [](Wg_Obj* value, void* ud) {
					std::vector<Wg_Obj*>& buf = *(std::vector<Wg_Obj*>*)ud;
					buf.push_back(value);
					return true;
					});
				if (!success)
					return nullptr;
			}
			
			return Wg_None(context);
		}

		static Wg_Obj* list_sort(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_LIST(0);

			Wg_Obj* kwargs = Wg_GetKwargs(context);
			if (kwargs == nullptr)
				return nullptr;

			Wg_Obj* kw[2]{};
			const char* keys[2] = { "reverse", "key" };
			if (!Wg_ParseKwargs(kwargs, keys, 2, kw))
				return nullptr;

			bool reverse = false;
			if (kw[0] != nullptr) {
				Wg_Obj* reverseValue = Wg_UnaryOp(WG_UOP_BOOL, kw[0]);
				if (reverseValue == nullptr)
					return nullptr;
				reverse = Wg_GetBool(reverseValue);
			}

			std::vector<Wg_Obj*> buf = argv[0]->Get<std::vector<Wg_Obj*>>();
			std::vector<Wg_ObjRef> refs;
			for (Wg_Obj* v : buf)
				refs.emplace_back(v);

			if (!MergeSort(buf.data(), buf.size(), kw[1]))
				return nullptr;

			if (reverse)
				std::reverse(buf.begin(), buf.end());
			
			argv[0]->Get<std::vector<Wg_Obj*>>() = std::move(buf);

			return Wg_None(context);
		}

		static Wg_Obj* list_reverse(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_LIST(0);

			auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();
			std::reverse(buf.begin(), buf.end());
			return Wg_None(context);
		}

		static Wg_Obj* map_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_MAP(0);

			auto it = std::find(context->reprStack.rbegin(), context->reprStack.rend(), argv[0]);
			if (it != context->reprStack.rend()) {
				return Wg_NewString(context, "{...}");
			} else {
				context->reprStack.push_back(argv[0]);
				const auto& buf = argv[0]->Get<wings::WDict>();
				std::string s = "{";
				for (const auto& [key, val] : buf) {
					Wg_Obj* k = Wg_UnaryOp(WG_UOP_REPR, key);
					if (k == nullptr) {
						context->reprStack.pop_back();
						return nullptr;
					}
					s += k->Get<std::string>() + ": ";
					
					Wg_Obj* v = Wg_UnaryOp(WG_UOP_REPR, val);
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
				return Wg_NewString(context, (s + "}").c_str());
			}
		}

		static Wg_Obj* map_nonzero(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_MAP(0);
			return Wg_NewBool(context, !argv[0]->Get<WDict>().empty());
		}

		static Wg_Obj* map_len(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_MAP(0);
			return Wg_NewInt(context, (Wg_int)argv[0]->Get<WDict>().size());
		}

		static Wg_Obj* map_contains(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_MAP(0);
			try {
				return Wg_NewBool(context, argv[0]->Get<WDict>().contains(argv[1]));
			} catch (HashException&) {
				return nullptr;
			}
		}

		static Wg_Obj* map_iter(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_MAP(0);
			return Wg_Call(context->builtins.dictKeysIter, argv, 1, nullptr);
		}

		static Wg_Obj* map_values(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_MAP(0);
			return Wg_Call(context->builtins.dictValuesIter, argv, 1, nullptr);
		}

		static Wg_Obj* map_items(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_MAP(0);
			return Wg_Call(context->builtins.dictItemsIter, argv, 1, nullptr);
		}

		static Wg_Obj* map_get(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(2, 3);
			WG_EXPECT_ARG_TYPE_MAP(0);

			auto& buf = argv[0]->Get<WDict>();
			WDict::iterator it;
			try {
				it = buf.find(argv[1]);
			} catch (HashException&) {
				return nullptr;
			}

			if (it == buf.end()) {
				return argc == 3 ? argv[2] : Wg_None(context);
			}

			return it->second;
		}

		static Wg_Obj* map_getitem(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_MAP(0);

			auto& buf = argv[0]->Get<WDict>();
			WDict::iterator it;
			try {
				it = buf.find(argv[1]);
			} catch (HashException&) {
				return nullptr;
			}

			if (it == buf.end()) {
				Wg_RaiseKeyError(context, argv[1]);
				return nullptr;
			}

			return it->second;
		}

		static Wg_Obj* map_setitem(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(3);
			WG_EXPECT_ARG_TYPE_MAP(0);

			try {
				argv[0]->Get<WDict>()[argv[1]] = argv[2];
			} catch (HashException&) {
				return nullptr;
			}
			return Wg_None(context);
		}

		static Wg_Obj* map_clear(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_MAP(0);
			argv[0]->Get<WDict>().clear();
			return Wg_None(context);
		}

		static Wg_Obj* map_copy(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_MAP(0);

			std::vector<Wg_Obj*> keys;
			std::vector<Wg_Obj*> values;
			for (const auto& [k, v] : argv[0]->Get<WDict>()) {
				keys.push_back(k);
				values.push_back(v);
			}
			return Wg_NewDictionary(context, keys.data(), values.data(), (int)keys.size());
		}

		static Wg_Obj* map_pop(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(2, 3);
			WG_EXPECT_ARG_TYPE_MAP(0);

			if (auto popped = argv[0]->Get<WDict>().erase(argv[1]))
				return popped.value();

			if (argc == 3)
				return argv[2];

			Wg_RaiseKeyError(context, argv[1]);
			return nullptr;
		}

		static Wg_Obj* map_popitem(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_MAP(0);

			auto& buf = argv[0]->Get<WDict>();
			if (buf.empty()) {
				Wg_RaiseKeyError(context);
				return nullptr;
			}

			auto popped = buf.pop();
			Wg_Obj* tupElems[2] = { popped.first, popped.second };
			return Wg_NewTuple(context, tupElems, 2);
		}

		static Wg_Obj* map_setdefault(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(2, 3);
			WG_EXPECT_ARG_TYPE_MAP(0);

			try {
				auto& entry = argv[0]->Get<WDict>()[argv[1]];
				if (entry == nullptr)
					entry = argc == 3 ? argv[2] : Wg_None(context);
				return entry;
			} catch (HashException&) {
				return nullptr;
			}
		}

		static Wg_Obj* map_update(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_MAP(0);

			Wg_Obj* iterable = argv[1];
			if (Wg_IsDictionary(argv[1])) {
				iterable = Wg_CallMethod(argv[1], "items", nullptr, 0);
			}

			auto f = [](Wg_Obj* obj, void* ud) {
				Wg_Obj* kv[2]{};
				if (!Wg_Unpack(obj, kv, 2))
					return false;
				
				Wg_ObjRef ref(kv[1]);
				try {
					((Wg_Obj*)ud)->Get<WDict>()[kv[0]] = kv[1];
				} catch (HashException&) {}
				return true;
			};
			
			if (Wg_Iterate(iterable, argv[0], f)) {
				return Wg_None(context);
			} else {
				return nullptr;
			}
		}

		static Wg_Obj* set_nonzero(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_SET(0);
			return Wg_NewBool(context, !argv[0]->Get<WSet>().empty());
		}

		static Wg_Obj* set_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_SET(0);

			auto it = std::find(context->reprStack.rbegin(), context->reprStack.rend(), argv[0]);
			if (it != context->reprStack.rend()) {
				return Wg_NewString(context, "{...}");
			} else {
				context->reprStack.push_back(argv[0]);
				const auto& buf = argv[0]->Get<wings::WSet>();

				if (buf.empty()) {
					context->reprStack.pop_back();
					return Wg_NewString(context, "set()");
				}

				std::string s = "{";
				for (Wg_Obj* val : buf) {
					Wg_Obj* v = Wg_UnaryOp(WG_UOP_REPR, val);
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
				return Wg_NewString(context, (s + "}").c_str());
			}
		}

		static Wg_Obj* set_iter(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_SET(0);
			return Wg_Call(context->builtins.setIter, argv, 1, nullptr);
		}

		static Wg_Obj* set_contains(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_SET(0);
			try {
				return Wg_NewBool(context, argv[0]->Get<WSet>().contains(argv[1]));
			} catch (HashException&) {
				Wg_ClearCurrentException(context);
				return Wg_NewBool(context, false);
			}
		}

		static Wg_Obj* set_len(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_SET(0);
			return Wg_NewInt(context, (int)argv[0]->Get<WSet>().size());
		}

		static Wg_Obj* set_clear(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_SET(0);
			argv[0]->Get<WSet>().clear();
			return Wg_None(context);
		}

		static Wg_Obj* set_copy(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_SET(0);
			return Wg_Call(context->builtins.set, argv, 1);
		}
		
		static Wg_Obj* set_add(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_SET(0);
			argv[0]->Get<WSet>().insert(argv[1]);
			return Wg_None(context);
		}

		static Wg_Obj* set_remove(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_SET(0);
			
			WSet::const_iterator it{};
			auto& set = argv[0]->Get<WSet>();
			try {
				it = set.find(argv[1]);
			} catch (HashException&) {
				return nullptr;
			}

			if (it == WSet::const_iterator{}) {
				Wg_RaiseKeyError(context, argv[1]);
				return nullptr;
			} else {
				set.erase(it);
				return Wg_None(context);
			}
		}

		static Wg_Obj* set_discard(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_SET(0);

			WSet::const_iterator it{};
			auto& set = argv[0]->Get<WSet>();
			try {
				it = set.find(argv[1]);
			} catch (HashException&) {
				return nullptr;
			}

			if (it != WSet::const_iterator{})
				set.erase(it);
			return Wg_None(context);
		}

		static Wg_Obj* set_pop(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_SET(0);
			auto& set = argv[0]->Get<WSet>();
			auto it = set.begin();
			if (it == set.end()) {
				Wg_RaiseKeyError(context);
				return nullptr;
			}
			Wg_Obj* obj = *it;
			set.erase(set.begin());
			return obj;
		}

		static Wg_Obj* set_update(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_SET(0);

			auto f = [](Wg_Obj* obj, void* ud) {
				auto set = (WSet*)ud;
				try {
					set->insert(obj);
				} catch (HashException&) {}
				return true;
			};

			if (!Wg_Iterate(argv[1], &argv[0]->Get<WSet>(), f))
				return nullptr;
			
			return Wg_None(context);
		}

		static Wg_Obj* set_union(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_AT_LEAST(1);
			WG_EXPECT_ARG_TYPE_SET(0);

			Wg_Obj* res = Wg_NewSet(context);
			Wg_ObjRef ref(res);
			
			auto f = [](Wg_Obj* obj, void* ud) {
				try {
					((WSet*)ud)->insert(obj);
				} catch (HashException&) {}
				return true;
			};

			for (int i = 0; i < argc; i++)
				if (!Wg_Iterate(argv[i], &res->Get<WSet>(), f))
					return nullptr;

			return res;
		}

		static Wg_Obj* set_difference(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_AT_LEAST(1);
			WG_EXPECT_ARG_TYPE_SET(0);

			Wg_Obj* res = Wg_NewSet(context);
			Wg_ObjRef ref(res);

			struct State {
				Wg_Obj** other;
				int otherCount;
				WSet* res;
			} s{ argv + 1, argc - 1, &res->Get<WSet>() };

			auto f = [](Wg_Obj* obj, void* ud) {
				auto s = (State*)ud;

				for (int i = 0; i < s->otherCount; i++) {
					Wg_Obj* contains = Wg_BinaryOp(WG_BOP_IN, obj, s->other[i]);
					if (contains == nullptr)
						return false;
					else if (Wg_GetBool(contains))
						return true;
				}

				try {
					s->res->insert(obj);
				} catch (HashException&) {}
				return true;
			};

			if (!Wg_Iterate(argv[0], &s, f))
				return nullptr;

			return res;
		}

		static Wg_Obj* set_intersection(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_AT_LEAST(1);
			WG_EXPECT_ARG_TYPE_SET(0);

			Wg_Obj* res = Wg_NewSet(context);
			Wg_ObjRef ref(res);

			struct State {
				Wg_Obj** other;
				int otherCount;
				WSet* res;
			} s{ argv + 1, argc - 1, &res->Get<WSet>() };

			auto f = [](Wg_Obj* obj, void* ud) {
				auto s = (State*)ud;

				for (int i = 0; i < s->otherCount; i++) {
					Wg_Obj* contains = Wg_BinaryOp(WG_BOP_IN, obj, s->other[i]);
					if (contains == nullptr)
						return false;
					else if (!Wg_GetBool(contains))
						return true;
				}

				try {
					s->res->insert(obj);
				} catch (HashException&) {}
				return true;
			};

			if (!Wg_Iterate(argv[0], &s, f))
				return nullptr;

			return res;
		}

		static Wg_Obj* set_symmetric_difference(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_SET(0);

			Wg_Obj* res = Wg_NewSet(context);
			Wg_ObjRef ref(res);

			struct State {
				Wg_Obj* other;
				WSet* res;
			} s = { nullptr, &res->Get<WSet>() };

			auto f = [](Wg_Obj* obj, void* ud) {
				auto s = (State*)ud;

				Wg_Obj* contains = Wg_BinaryOp(WG_BOP_IN, obj, s->other);
				if (contains == nullptr)
					return false;
				else if (Wg_GetBool(contains))
					return true;
					
				try {
					s->res->insert(obj);
				} catch (HashException&) {}
				return true;
			};

			s.other = argv[1];
			if (!Wg_Iterate(argv[0], &s, f))
				return nullptr;
			s.other = argv[0];
			if (!Wg_Iterate(argv[1], &s, f))
				return nullptr;

			return res;
		}

		static Wg_Obj* set_isdisjoint(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_SET(0);

			Wg_Obj* inters = Wg_CallMethod(argv[0], "intersection", argv + 1, 1);
			if (inters == nullptr)
				return nullptr;

			return Wg_UnaryOp(WG_UOP_NOT, inters);
		}

		static Wg_Obj* set_issubset(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_SET(0);

			size_t size = argv[0]->Get<WSet>().size();

			Wg_Obj* inters = Wg_CallMethod(argv[0], "intersection", argv + 1, 1);
			if (inters == nullptr)
				return nullptr;
			
			if (!Wg_IsSet(inters)) {
				return Wg_NewBool(context, false);
			}

			return Wg_NewBool(context, inters->Get<WSet>().size() == size);
		}

		static Wg_Obj* set_issuperset(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_SET(0);

			struct State {
				Wg_Obj* self;
				bool result;
			} s = { argv[0], true };

			auto f = [](Wg_Obj* obj, void* ud) {
				auto s = (State*)ud;
				Wg_Obj* contains = Wg_BinaryOp(WG_BOP_IN, obj, s->self);
				if (contains && !Wg_GetBool(contains)) {
					s->result = false;
					return false;
				}
				return true;
			};

			if (!Wg_Iterate(argv[1], &s, f) && s.result)
				return nullptr;
			
			return Wg_NewBool(context, s.result);
		}

		static Wg_Obj* func_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_FUNC(0);
			std::string s = "<function at " + PtrToString(argv[0]) + ">";
			return Wg_NewString(context, s.c_str());
		}

		static Wg_Obj* BaseException_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			return Wg_GetAttribute(argv[0], "_message");
		}

		static Wg_Obj* DictKeysIter_next(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WDict::iterator* it{};
			if (!TryGetUserdata(argv[0], "__DictKeysIter", &it)) {
				Wg_RaiseArgumentTypeError(context, 0, "__DictKeysIter");
				return nullptr;
			}
			
			it->Revalidate();
			if (*it == WDict::iterator{}) {
				Wg_RaiseException(context, WG_EXC_STOPITERATION);
				return nullptr;
			}
			
			Wg_Obj* key = (*it)->first;
			++it;
			return key;
		}

		static Wg_Obj* DictValuesIter_next(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WDict::iterator* it{};
			if (!TryGetUserdata(argv[0], "__DictValuesIter", &it)) {
				Wg_RaiseArgumentTypeError(context, 0, "__DictValuesIter");
				return nullptr;
			}

			it->Revalidate();
			if (*it == WDict::iterator{}) {
				Wg_RaiseException(context, WG_EXC_STOPITERATION);
				return nullptr;
			}

			Wg_Obj* value = (*it)->second;
			++it;
			return value;
		}

		static Wg_Obj* DictItemsIter_next(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WDict::iterator* it{};
			if (!TryGetUserdata(argv[0], "__DictItemsIter", &it)) {
				Wg_RaiseArgumentTypeError(context, 0, "__DictItemsIter");
				return nullptr;
			}
			
			it->Revalidate();
			if (*it == WDict::iterator{}) {
				Wg_RaiseException(context, WG_EXC_STOPITERATION);
				return nullptr;
			}

			Wg_Obj* tup[2] = { (*it)->first, (*it)->second };
			++it;
			return Wg_NewTuple(context, tup, 2);
		}

		static Wg_Obj* SetIter_next(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WSet::iterator* it{};
			if (!TryGetUserdata(argv[0], "__SetIter", &it)) {
				Wg_RaiseArgumentTypeError(context, 0, "__SetIter");
				return nullptr;
			}
			
			it->Revalidate();
			if (*it == WSet::iterator{}) {
				Wg_RaiseException(context, WG_EXC_STOPITERATION);
				return nullptr;
			}
			
			Wg_Obj* obj = **it;
			++it;
			return obj;
		}

		static Wg_Obj* File_iter(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			if (!Wg_TryGetUserdata(argv[0], "__File", nullptr)) {
				Wg_RaiseArgumentTypeError(context, 0, "__File");
				return nullptr;
			}

			return Wg_Call(context->builtins.readlineIter, argv, 1);
		}

		static Wg_Obj* File_read(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 2);
			std::fstream* f{};
			if (!TryGetUserdata(argv[0], "__File", &f)) {
				Wg_RaiseArgumentTypeError(context, 0, "__File");
				return nullptr;
			}

			Wg_int size = -1;
			if (argc == 2) {
				WG_EXPECT_ARG_TYPE_INT(1);
				size = Wg_GetInt(argv[1]);
			}

			if (size < 0) {
				auto cur = f->tellg();
				f->seekg(0, std::ios::end);
				size = (Wg_int)(f->tellg() - cur);
				f->seekg(cur);
			}
			
			std::vector<char> buf((size_t)size);
			f->read(buf.data(), size);
			return Wg_NewStringBuffer(context, buf.data(), (int)buf.size());
		}

		static Wg_Obj* File_readline(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			std::fstream* f{};
			if (!TryGetUserdata(argv[0], "__File", &f)) {
				Wg_RaiseArgumentTypeError(context, 0, "__File");
				return nullptr;
			}
			
			if (f->eof()) {
				return Wg_NewString(context);
			}

			std::string s;
			std::getline(*f, s);
			if (f->good())
				s += '\n';
			return Wg_NewString(context, s.c_str());
		}

		static Wg_Obj* File_readlines(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			if (!Wg_TryGetUserdata(argv[0], "__File", nullptr)) {
				Wg_RaiseArgumentTypeError(context, 0, "__File");
				return nullptr;
			}

			return Wg_Call(context->builtins.list, argv, 1);
		}

		static Wg_Obj* File_closex(Wg_Context* context, Wg_Obj** argv, int argc) {
			std::fstream* f{};
			if (!TryGetUserdata(argv[0], "__File", &f)) {
				Wg_RaiseArgumentTypeError(context, 0, "__File");
				return nullptr;
			}

			if (f->is_open())
				f->close();

			return Wg_None(context);
		}

		static Wg_Obj* File_close(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			return File_closex(context, argv, argc);
		}

		static Wg_Obj* File_exit(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(4);
			return File_closex(context, argv, argc);
		}

		static Wg_Obj* File_seekable(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			if (!Wg_TryGetUserdata(argv[0], "__File", nullptr)) {
				Wg_RaiseArgumentTypeError(context, 0, "__File");
				return nullptr;
			}
			
			return Wg_NewBool(context, true);
		}

		static Wg_Obj* File_readable(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			if (!Wg_TryGetUserdata(argv[0], "__File", nullptr)) {
				Wg_RaiseArgumentTypeError(context, 0, "__File");
				return nullptr;
			}

			return Wg_GetAttribute(argv[0], "_readable");
		}

		static Wg_Obj* File_writable(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			if (!Wg_TryGetUserdata(argv[0], "__File", nullptr)) {
				Wg_RaiseArgumentTypeError(context, 0, "__File");
				return nullptr;
			}

			return Wg_GetAttribute(argv[0], "_writable");
		}

		static Wg_Obj* File_seek(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_INT(1);
			std::fstream* f{};
			if (!TryGetUserdata(argv[0], "__File", &f)) {
				Wg_RaiseArgumentTypeError(context, 0, "__File");
				return nullptr;
			}
			
			f->seekg((std::streampos)Wg_GetInt(argv[1]));

			return Wg_None(context);
		}

		static Wg_Obj* File_tell(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			std::fstream* f{};
			if (!TryGetUserdata(argv[0], "__File", &f)) {
				Wg_RaiseArgumentTypeError(context, 0, "__File");
				return nullptr;
			}

			return Wg_NewInt(context, (Wg_int)f->tellg());
		}

		static Wg_Obj* File_flush(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			std::fstream* f{};
			if (!TryGetUserdata(argv[0], "__File", &f)) {
				Wg_RaiseArgumentTypeError(context, 0, "__File");
				return nullptr;
			}

			f->flush();

			return Wg_None(context);
		}

		static Wg_Obj* File_write(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_STRING(1);
			std::fstream* f{};
			if (!TryGetUserdata(argv[0], "__File", &f)) {
				Wg_RaiseArgumentTypeError(context, 0, "__File");
				return nullptr;
			}

			int len{};
			const char* s = Wg_GetString(argv[1], &len);
			f->write(s, (std::streamsize)len);

			return Wg_None(context);
		}

		static Wg_Obj* File_writelines(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			if (!Wg_TryGetUserdata(argv[0], "__File", nullptr)) {
				Wg_RaiseArgumentTypeError(context, 0, "__File");
				return nullptr;
			}

			auto fn = [](Wg_Obj* obj, void* ud) {
				auto* file = (Wg_Obj*)ud;
				return Wg_CallMethod(file, "write", &obj, 1) != nullptr;
			};

			if (!Wg_Iterate(argv[1], argv[0], fn))
				return nullptr;

			return Wg_None(context);
		}
		
		static Wg_Obj* self(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			return argv[0];
		}

	} // namespace methods

	namespace lib {

		template <size_t base>
		static Wg_Obj* base_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);

			Wg_Obj* val = Wg_UnaryOp(WG_UOP_INDEX, argv[0]);
			if (val == nullptr)
				return nullptr;
			
			Wg_int i = Wg_GetInt(val);			
			std::string s;

			if constexpr (base == 2) {
				s = "0b";
			} else if constexpr (base == 8) {
				s = "0o";
			} else if constexpr (base == 16) {
				s = "0x";
			}
			
			do {
				s += "0123456789abcdef"[i % base];
				i /= base;
			}
			while (i > 0);

			return Wg_NewString(context, s.c_str());
		}

		static Wg_Obj* callable(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			
			if (Wg_IsFunction(argv[0])) {
				return Wg_NewBool(context, true);
			} else {
				return Wg_NewBool(context, Wg_HasAttribute(argv[0], "__call__"));
			}
		}

		static Wg_Obj* chr(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_INT(0);
			
			Wg_int i = Wg_GetInt(argv[0]);
			char s[2] = { (char)i, 0 };
			return Wg_NewString(context, s);
		}

		static Wg_Obj* compile(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(3);
			WG_EXPECT_ARG_TYPE_STRING(0);
			WG_EXPECT_ARG_TYPE_STRING(1);
			WG_EXPECT_ARG_TYPE_STRING(2);
		
			const char* source = Wg_GetString(argv[0]);
			const char* filename = Wg_GetString(argv[1]);
			const char* mode = Wg_GetString(argv[2]);
			
			Wg_Obj* fn = nullptr;
			if (std::strcmp(mode, "exec")) {
				fn = Wg_Compile(context, source, filename);
			} else if (std::strcmp(mode, "eval")) {
				fn = Wg_CompileExpression(context, source, filename);
			} else {
				Wg_RaiseException(context, WG_EXC_VALUEERROR, "compile() mode must be 'exec' or 'eval'");
			}
			
			if (fn == nullptr)
				return nullptr;

			return Wg_Call(context->builtins.codeObject, &fn, 1);
		}

		static Wg_Obj* eval(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			
			if (Wg_IsInstance(argv[0], &context->builtins.codeObject, 1)) {
				return Wg_CallMethod(argv[0], "f", nullptr, 0);
			} else {
				WG_EXPECT_ARG_TYPE_STRING(0);
				const char* source = Wg_GetString(argv[0]);
				return Wg_ExecuteExpression(context, source, "<string>");
			}
		}

		static Wg_Obj* exec(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);

			if (Wg_IsInstance(argv[0], &context->builtins.codeObject, 1)) {
				if (Wg_CallMethod(argv[0], "f", nullptr, 0) == nullptr)
					return nullptr;
			} else {
				WG_EXPECT_ARG_TYPE_STRING(0);
				const char* source = Wg_GetString(argv[0]);
				return Wg_Execute(context, source, "<string>");
			}
			return Wg_None(context);
		}

		static Wg_Obj* exit(Wg_Context* context, Wg_Obj** argv, int argc) {
			Wg_RaiseException(context, WG_EXC_SYSTEMEXIT);
			return nullptr;
		}

		static Wg_Obj* getattr(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			WG_EXPECT_ARG_TYPE_STRING(1);
			
			const char* name = Wg_GetString(argv[1]);
			return Wg_GetAttribute(argv[0], name);
		}
		
		static Wg_Obj* id(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			return Wg_NewInt(context, (Wg_int)argv[0]);
		}

		static Wg_Obj* input(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(0, 1);
			
			if (argc == 1) {
				Wg_Obj* prompt = Wg_UnaryOp(WG_UOP_STR, argv[0]);
				if (prompt == nullptr)
					return nullptr;
				
				Wg_PrintString(context, Wg_GetString(prompt));
			}
			
			std::string s;
			std::getline(std::cin, s);
			
			return Wg_NewString(context, s.c_str());
		}

		static Wg_Obj* isinstance(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			bool ret{};
			if (Wg_IsTuple(argv[1])) {
				const auto& buf = argv[1]->Get<std::vector<Wg_Obj*>>();
				ret = Wg_IsInstance(argv[0], buf.data(), (int)buf.size()) != nullptr;
			} else {
				ret = Wg_IsInstance(argv[0], argv + 1, 1) != nullptr;
			}
			return Wg_NewBool(context, ret);
		}

		static Wg_Obj* ord(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(1);
			WG_EXPECT_ARG_TYPE_STRING(0);

			const char* s = Wg_GetString(argv[0]);
			if (s[0] == 0) {
				Wg_RaiseException(context, WG_EXC_VALUEERROR, "ord() arg is an empty string");
				return nullptr;
			} else if (s[1] == 0) {
				return Wg_NewInt(context, (int)s[0]);
			} else {
				Wg_RaiseException(context, WG_EXC_VALUEERROR, "ord() arg is not a single character");
				return nullptr;
			}
		}

		static Wg_Obj* pow(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(2);
			return Wg_BinaryOp(WG_BOP_POW, argv[0], argv[1]);
		}

		static Wg_Obj* print(Wg_Context* context, Wg_Obj** argv, int argc) {
			std::string text;
			for (int i = 0; i < argc; i++) {
				if (Wg_Obj* s = Wg_UnaryOp(WG_UOP_STR, argv[i])) {
					text += Wg_GetString(s);
				} else {
					return nullptr;
				}

				if (i < argc - 1) {
					text += ' ';
				}
			}
			text += '\n';
			Wg_Print(context, text.c_str(), (int)text.size());
			return Wg_None(context);
		}

		static Wg_Obj* round(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT_BETWEEN(1, 2);
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);

			Wg_float f = Wg_GetFloat(argv[0]);
			
			Wg_float m = 1;
			bool dpSpecified = false;
			if (argc == 2 && !Wg_IsNone(argv[1])) {
				WG_EXPECT_ARG_TYPE_INT(1);
				m = std::pow(10, Wg_GetInt(argv[1]));
				dpSpecified = true;
			}
			
			auto r = std::lrint(f * m) / m;
			if (!dpSpecified || Wg_IsInt(argv[0])) {
				return Wg_NewInt(context, (Wg_int)r);
			} else {
				return Wg_NewFloat(context, (Wg_float)r);
			}
		}

		static Wg_Obj* setattr(Wg_Context* context, Wg_Obj** argv, int argc) {
			WG_EXPECT_ARG_COUNT(3);
			WG_EXPECT_ARG_TYPE_STRING(1);

			const char* name = Wg_GetString(argv[1]);
			Wg_SetAttribute(argv[0], name, argv[2]);
			return Wg_None(context);
		}

	} // namespace lib

	bool ImportBuiltins(Wg_Context* context) {
		try {
			auto getGlobal = [&](const char* name) {
				if (Wg_Obj* v = Wg_GetGlobal(context, name))
					return v;
				throw LibraryInitException();
			};

			auto createClass = [&](const char* name, Wg_Obj* base = nullptr, bool assign = true) {
				if (Wg_Obj* v = Wg_NewClass(context, name, &base, base ? 1 : 0)) {
					if (assign)
						Wg_SetGlobal(context, name, v);
					return v;
				}
				throw LibraryInitException();
			};

			auto& b = context->builtins;

			// Create object class
			b.object = Alloc(context);
			if (b.object == nullptr)
				throw LibraryInitException();
			b.object->type = "__class";
			b.object->data = new Wg_Obj::Class{ std::string("object") };
			b.object->finalizer.fptr = [](Wg_Obj* obj, void*) { delete (Wg_Obj::Class*)obj->data; };
			b.object->Get<Wg_Obj::Class>().instanceAttributes.Set("__class__", b.object);
			b.object->attributes.AddParent(b.object->Get<Wg_Obj::Class>().instanceAttributes);
			b.object->Get<Wg_Obj::Class>().userdata = context;
			b.object->Get<Wg_Obj::Class>().ctor = ctors::object;
			Wg_SetGlobal(context, "object", b.object);

			// Create function class
			b.func = Alloc(context);
			if (b.func == nullptr)
				throw LibraryInitException();
			b.func->type = "__class";
			b.func->data = new Wg_Obj::Class{ std::string("function") };
			b.func->finalizer.fptr = [](Wg_Obj* obj, void*) { delete (Wg_Obj::Class*)obj->data; };
			b.func->Get<Wg_Obj::Class>().instanceAttributes.Set("__class__", b.func);
			b.func->attributes.AddParent(b.object->Get<Wg_Obj::Class>().instanceAttributes);
			b.func->Get<Wg_Obj::Class>().userdata = context;
			b.func->Get<Wg_Obj::Class>().ctor = ctors::func;
			RegisterMethod(b.func, "__str__", methods::func_str);

			// Create tuple class
			b.tuple = Alloc(context);
			b.tuple->type = "__class";
			b.tuple->data = new Wg_Obj::Class{ std::string("tuple") };
			b.tuple->finalizer.fptr = [](Wg_Obj* obj, void*) { delete (Wg_Obj::Class*)obj->data; };
			b.tuple->Get<Wg_Obj::Class>().instanceAttributes.Set("__class__", b.tuple);
			b.tuple->attributes.AddParent(b.object->Get<Wg_Obj::Class>().instanceAttributes);
			b.tuple->Get<Wg_Obj::Class>().userdata = context;
			b.tuple->Get<Wg_Obj::Class>().ctor = ctors::tuple;
			Wg_SetGlobal(context, "tuple", b.tuple);
			RegisterMethod(b.tuple, "__iter__", methods::object_iter);
			RegisterMethod(b.tuple, "__str__", methods::collection_str<Collection::Tuple>);
			RegisterMethod(b.tuple, "__getitem__", methods::collection_getitem<Collection::Tuple>);
			RegisterMethod(b.tuple, "__len__", methods::collection_len<Collection::Tuple>);
			RegisterMethod(b.tuple, "__contains__", methods::collection_contains<Collection::Tuple>);
			RegisterMethod(b.tuple, "__eq__", methods::collection_eq<Collection::Tuple>);
			RegisterMethod(b.tuple, "__lt__", methods::collection_lt<Collection::Tuple>);
			RegisterMethod(b.tuple, "__nonzero__", methods::collection_nonzero<Collection::Tuple>);
			RegisterMethod(b.tuple, "count", methods::collection_count<Collection::Tuple>);
			RegisterMethod(b.tuple, "index", methods::collection_index<Collection::Tuple>);

			// Create NoneType class
			b.none = Alloc(context);
			b.none->type = "__class";
			b.none->data = new Wg_Obj::Class{ std::string("NoneType") };
			b.none->finalizer.fptr = [](Wg_Obj* obj, void*) { delete (Wg_Obj::Class*)obj->data; };
			b.none->Get<Wg_Obj::Class>().instanceAttributes.Set("__class__", b.none);
			b.none->attributes.AddParent(b.object->Get<Wg_Obj::Class>().instanceAttributes);
			b.none->Get<Wg_Obj::Class>().userdata = context;
			b.none->Get<Wg_Obj::Class>().ctor = ctors::none;

			// Create None singleton
			b.none = Alloc(context);
			b.none->type = "__null";
			Wg_SetAttribute(b.none, "__class__", b.none);
			b.none->attributes.AddParent(b.object->Get<Wg_Obj::Class>().instanceAttributes);
			RegisterMethod(b.none, "__nonzero__", methods::null_nonzero);
			RegisterMethod(b.none, "__str__", methods::null_str);

			// Add __bases__ tuple to the classes created before
			Wg_Obj* emptyTuple = Wg_NewTuple(context, nullptr, 0);
			if (emptyTuple == nullptr)
				throw LibraryInitException();
			Wg_SetAttribute(b.object, "__bases__", emptyTuple);
			Wg_SetAttribute(b.none, "__bases__", emptyTuple);
			Wg_SetAttribute(b.func, "__bases__", emptyTuple);
			Wg_SetAttribute(b.tuple, "__bases__", emptyTuple);

			// Add methods
			RegisterMethod(b.object, "__pos__", methods::self);
			RegisterMethod(b.object, "__str__", methods::object_str);
			RegisterMethod(b.object, "__nonzero__", methods::object_nonzero);
			RegisterMethod(b.object, "__repr__", methods::object_repr);
			RegisterMethod(b.object, "__eq__", methods::object_eq);
			RegisterMethod(b.object, "__ne__", methods::object_ne);
			RegisterMethod(b.object, "__le__", methods::object_le);
			RegisterMethod(b.object, "__gt__", methods::object_gt);
			RegisterMethod(b.object, "__ge__", methods::object_ge);
			RegisterMethod(b.object, "__iadd__", methods::object_iadd);
			RegisterMethod(b.object, "__isub__", methods::object_isub);
			RegisterMethod(b.object, "__imul__", methods::object_imul);
			RegisterMethod(b.object, "__itruediv__", methods::object_itruediv);
			RegisterMethod(b.object, "__ifloordiv__", methods::object_ifloordiv);
			RegisterMethod(b.object, "__imod__", methods::object_imod);
			RegisterMethod(b.object, "__ipow__", methods::object_ipow);
			RegisterMethod(b.object, "__iand__", methods::object_iand);
			RegisterMethod(b.object, "__ior__", methods::object_ior);
			RegisterMethod(b.object, "__ixor__", methods::object_ixor);
			RegisterMethod(b.object, "__ilshift__", methods::object_ilshift);
			RegisterMethod(b.object, "__irshift__", methods::object_irshift);
			RegisterMethod(b.object, "__hash__", methods::object_hash);
			RegisterMethod(b.object, "__iter__", methods::object_iter);
			RegisterMethod(b.object, "__reversed__", methods::object_reversed);

			b._bool = createClass("bool");
			b._bool->Get<Wg_Obj::Class>().ctor = ctors::_bool;
			RegisterMethod(b._bool, "__nonzero__", methods::self);
			RegisterMethod(b._bool, "__int__", methods::bool_int);
			RegisterMethod(b._bool, "__float__", methods::bool_float);
			RegisterMethod(b._bool, "__str__", methods::bool_str);
			RegisterMethod(b._bool, "__eq__", methods::bool_eq);
			RegisterMethod(b._bool, "__hash__", methods::bool_hash);
			RegisterMethod(b._bool, "__abs__", methods::bool_abs);

			Wg_Obj* _false = Alloc(context);
			if (_false == nullptr)
				throw LibraryInitException();
			_false->attributes = b._bool->Get<Wg_Obj::Class>().instanceAttributes.Copy();
			_false->type = "__bool";
			_false->data = new bool(false);
			_false->finalizer.fptr = [](Wg_Obj* obj, void*) { delete (bool*)obj->data; };
			b._false = _false;
			Wg_Obj* _true = Alloc(context);
			if (_true == nullptr)
				throw LibraryInitException();
			_true->attributes = b._bool->Get<Wg_Obj::Class>().instanceAttributes.Copy();
			_true->type = "__bool";
			_true->data = new bool(true);
			_true->finalizer.fptr = [](Wg_Obj* obj, void*) { delete (bool*)obj->data; };
			b._true = _true;

			b._int = createClass("int");
			RegisterMethod(b._int, "__init__", ctors::_int);
			RegisterMethod(b._int, "__nonzero__", methods::int_nonzero);
			RegisterMethod(b._int, "__int__", methods::self);
			RegisterMethod(b._int, "__float__", methods::int_float);
			RegisterMethod(b._int, "__str__", methods::int_str);
			RegisterMethod(b._int, "__index__", methods::self);
			RegisterMethod(b._int, "__neg__", methods::int_neg);
			RegisterMethod(b._int, "__add__", methods::int_add);
			RegisterMethod(b._int, "__sub__", methods::int_sub);
			RegisterMethod(b._int, "__mul__", methods::int_mul);
			RegisterMethod(b._int, "__truediv__", methods::int_truediv);
			RegisterMethod(b._int, "__floordiv__", methods::int_floordiv);
			RegisterMethod(b._int, "__mod__", methods::int_mod);
			RegisterMethod(b._int, "__pow__", methods::int_pow);
			RegisterMethod(b._int, "__and__", methods::int_and);
			RegisterMethod(b._int, "__or__", methods::int_or);
			RegisterMethod(b._int, "__xor__", methods::int_xor);
			RegisterMethod(b._int, "__invert__", methods::int_invert);
			RegisterMethod(b._int, "__lshift__", methods::int_lshift);
			RegisterMethod(b._int, "__rshift__", methods::int_rshift);
			RegisterMethod(b._int, "__lt__", methods::int_lt);
			RegisterMethod(b._int, "__eq__", methods::int_eq);
			RegisterMethod(b._int, "__hash__", methods::int_hash);
			RegisterMethod(b._int, "__abs__", methods::int_abs);
			RegisterMethod(b._int, "bit_length", methods::int_bit_length);
			RegisterMethod(b._int, "bit_count", methods::int_bit_count);

			b._float = createClass("float");
			RegisterMethod(b._float, "__init__", ctors::_float);
			RegisterMethod(b._float, "__nonzero__", methods::float_nonzero);
			RegisterMethod(b._float, "__int__", methods::float_int);
			RegisterMethod(b._float, "__float__", methods::self);
			RegisterMethod(b._float, "__str__", methods::float_str);
			RegisterMethod(b._float, "__neg__", methods::float_neg);
			RegisterMethod(b._float, "__add__", methods::float_add);
			RegisterMethod(b._float, "__sub__", methods::float_sub);
			RegisterMethod(b._float, "__mul__", methods::float_mul);
			RegisterMethod(b._float, "__truediv__", methods::float_truediv);
			RegisterMethod(b._float, "__floordiv__", methods::float_floordiv);
			RegisterMethod(b._float, "__mod__", methods::float_mod);
			RegisterMethod(b._float, "__pow__", methods::float_pow);
			RegisterMethod(b._float, "__lt__", methods::float_lt);
			RegisterMethod(b._float, "__eq__", methods::float_eq);
			RegisterMethod(b._float, "__hash__", methods::float_hash);
			RegisterMethod(b._float, "__abs__", methods::float_abs);
			RegisterMethod(b._float, "is_integer", methods::float_is_integer);

			b.str = createClass("str");
			RegisterMethod(b.str, "__init__", ctors::str);
			RegisterMethod(b.str, "__nonzero__", methods::str_nonzero);
			RegisterMethod(b.str, "__int__", methods::str_int);
			RegisterMethod(b.str, "__float__", methods::str_float);
			RegisterMethod(b.str, "__str__", methods::self);
			RegisterMethod(b.str, "__repr__", methods::str_repr);
			RegisterMethod(b.str, "__len__", methods::str_len);
			RegisterMethod(b.str, "__add__", methods::str_add);
			RegisterMethod(b.str, "__mul__", methods::str_mul);
			RegisterMethod(b.str, "__getitem__", methods::str_getitem);
			RegisterMethod(b.str, "__contains__", methods::str_contains);
			RegisterMethod(b.str, "__lt__", methods::str_lt);
			RegisterMethod(b.str, "__eq__", methods::str_eq);
			RegisterMethod(b.str, "__hash__", methods::str_hash);
			RegisterMethod(b.str, "capitalize", methods::str_capitalize);
			RegisterMethod(b.str, "casefold", methods::str_casefold);
			RegisterMethod(b.str, "lower", methods::str_lower);
			RegisterMethod(b.str, "upper", methods::str_upper);
			RegisterMethod(b.str, "center", methods::str_center);
			RegisterMethod(b.str, "count", methods::str_count);
			RegisterMethod(b.str, "format", methods::str_format);
			RegisterMethod(b.str, "find", methods::str_find);
			RegisterMethod(b.str, "index", methods::str_index);
			RegisterMethod(b.str, "startswith", methods::str_startswith);
			RegisterMethod(b.str, "endswith", methods::str_endswith);
			RegisterMethod(b.str, "isalnum", methods::str_isalnum);
			RegisterMethod(b.str, "isalpha", methods::str_isalpha);
			RegisterMethod(b.str, "isascii", methods::str_isascii);
			RegisterMethod(b.str, "isdecimal", methods::str_isdecimal);
			RegisterMethod(b.str, "isdigit", methods::str_isdigit);
			RegisterMethod(b.str, "isidentifier", methods::str_isidentifier);
			RegisterMethod(b.str, "islower", methods::str_islower);
			RegisterMethod(b.str, "isupper", methods::str_isupper);
			RegisterMethod(b.str, "isnumeric", methods::str_isnumeric);
			RegisterMethod(b.str, "isprintable", methods::str_isprintable);
			RegisterMethod(b.str, "isspace", methods::str_isspace);
			RegisterMethod(b.str, "join", methods::str_join);
			RegisterMethod(b.str, "ljust", methods::str_ljust);
			RegisterMethod(b.str, "lstrip", methods::str_lstrip);
			RegisterMethod(b.str, "replace", methods::str_replace);
			RegisterMethod(b.str, "rfind", methods::str_rfind);
			RegisterMethod(b.str, "rindex", methods::str_rindex);
			RegisterMethod(b.str, "rjust", methods::str_rjust);
			RegisterMethod(b.str, "rstrip", methods::str_rstrip);
			RegisterMethod(b.str, "split", methods::str_split);
			RegisterMethod(b.str, "splitlines", methods::str_splitlines);
			RegisterMethod(b.str, "strip", methods::str_strip);
			RegisterMethod(b.str, "zfill", methods::str_zfill);

			b.list = createClass("list");
			RegisterMethod(b.list, "__init__", ctors::list);
			RegisterMethod(b.list, "__nonzero__", methods::collection_nonzero<Collection::List>);
			RegisterMethod(b.list, "__str__", methods::collection_str<Collection::List>);
			RegisterMethod(b.list, "__len__", methods::collection_len<Collection::List>);
			RegisterMethod(b.list, "__getitem__", methods::collection_getitem<Collection::List>);
			RegisterMethod(b.list, "__setitem__", methods::list_setitem);
			RegisterMethod(b.list, "__contains__", methods::collection_contains<Collection::List>);
			RegisterMethod(b.list, "__eq__", methods::collection_eq<Collection::List>);
			RegisterMethod(b.list, "__lt__", methods::collection_lt<Collection::List>);
			RegisterMethod(b.list, "count", methods::collection_count<Collection::List>);
			RegisterMethod(b.list, "index", methods::collection_index<Collection::List>);
			RegisterMethod(b.list, "append", methods::list_append);
			RegisterMethod(b.list, "clear", methods::list_clear);
			RegisterMethod(b.list, "copy", methods::list_copy);
			RegisterMethod(b.list, "extend", methods::list_extend);
			RegisterMethod(b.list, "insert", methods::list_insert);
			RegisterMethod(b.list, "pop", methods::list_pop);
			RegisterMethod(b.list, "remove", methods::list_remove);
			RegisterMethod(b.list, "reverse", methods::list_reverse);
			RegisterMethod(b.list, "sort", methods::list_sort);

			b.dict = createClass("dict");
			RegisterMethod(b.dict, "__init__", ctors::map);
			RegisterMethod(b.dict, "__nonzero__", methods::map_nonzero);
			RegisterMethod(b.dict, "__str__", methods::map_str);
			RegisterMethod(b.dict, "__contains__", methods::map_contains);
			RegisterMethod(b.dict, "__getitem__", methods::map_getitem);
			RegisterMethod(b.dict, "__iter__", methods::map_iter);
			RegisterMethod(b.dict, "__len__", methods::map_len);
			RegisterMethod(b.dict, "__setitem__", methods::map_setitem);
			RegisterMethod(b.dict, "clear", methods::map_clear);
			RegisterMethod(b.dict, "copy", methods::map_copy);
			RegisterMethod(b.dict, "get", methods::map_get);
			RegisterMethod(b.dict, "keys", methods::map_iter);
			RegisterMethod(b.dict, "values", methods::map_values);
			RegisterMethod(b.dict, "items", methods::map_items);
			RegisterMethod(b.dict, "pop", methods::map_pop);
			RegisterMethod(b.dict, "popitem", methods::map_popitem);
			RegisterMethod(b.dict, "setdefault", methods::map_setdefault);
			RegisterMethod(b.dict, "update", methods::map_update);

			b.set = createClass("set");
			RegisterMethod(b.set, "__init__", ctors::set);
			RegisterMethod(b.set, "__nonzero__", methods::set_nonzero);
			RegisterMethod(b.set, "__str__", methods::set_str);
			RegisterMethod(b.set, "__contains__", methods::set_contains);
			RegisterMethod(b.set, "__iter__", methods::set_iter);
			RegisterMethod(b.set, "__len__", methods::set_len);
			RegisterMethod(b.set, "add", methods::set_add);
			RegisterMethod(b.set, "clear", methods::set_clear);
			RegisterMethod(b.set, "copy", methods::set_copy);
			RegisterMethod(b.set, "difference", methods::set_difference);
			RegisterMethod(b.set, "discard", methods::set_discard);
			RegisterMethod(b.set, "intersection", methods::set_intersection);
			RegisterMethod(b.set, "isdisjoint", methods::set_isdisjoint);
			RegisterMethod(b.set, "issubset", methods::set_issubset);
			RegisterMethod(b.set, "issuperset", methods::set_issuperset);
			RegisterMethod(b.set, "pop", methods::set_pop);
			RegisterMethod(b.set, "remove", methods::set_remove);
			RegisterMethod(b.set, "symmetric_difference", methods::set_symmetric_difference);
			RegisterMethod(b.set, "union", methods::set_union);
			RegisterMethod(b.set, "update", methods::set_update);

			b.dictKeysIter = createClass("__DictKeysIter", nullptr, false);
			RegisterMethod(b.dictKeysIter, "__init__", ctors::DictIter);
			RegisterMethod(b.dictKeysIter, "__next__", methods::DictKeysIter_next);
			RegisterMethod(b.dictKeysIter, "__iter__", methods::self);

			b.dictValuesIter = createClass("__DictValuesIter", nullptr, false);
			RegisterMethod(b.dictValuesIter, "__init__", ctors::DictIter);
			RegisterMethod(b.dictValuesIter, "__next__", methods::DictValuesIter_next);
			RegisterMethod(b.dictValuesIter, "__iter__", methods::self);

			b.dictItemsIter = createClass("__DictItemsIter", nullptr, false);
			RegisterMethod(b.dictItemsIter, "__init__", ctors::DictIter);
			RegisterMethod(b.dictItemsIter, "__next__", methods::DictItemsIter_next);
			RegisterMethod(b.dictItemsIter, "__iter__", methods::self);

			b.setIter = createClass("__SetIter", nullptr, false);
			RegisterMethod(b.setIter, "__init__", ctors::SetIter);
			RegisterMethod(b.setIter, "__next__", methods::SetIter_next);
			RegisterMethod(b.setIter, "__iter__", methods::self);

			b.file = createClass("__File", nullptr, false);
			RegisterMethod(b.file, "__init__", ctors::File);
			RegisterMethod(b.file, "__iter__", methods::File_iter);
			RegisterMethod(b.file, "__enter__", methods::self);
			RegisterMethod(b.file, "__exit__", methods::File_exit);
			RegisterMethod(b.file, "close", methods::File_close);
			RegisterMethod(b.file, "read", methods::File_read);
			RegisterMethod(b.file, "readline", methods::File_readline);
			RegisterMethod(b.file, "readlines", methods::File_readlines);
			RegisterMethod(b.file, "write", methods::File_write);
			RegisterMethod(b.file, "writelines", methods::File_writelines);
			RegisterMethod(b.file, "readable", methods::File_readable);
			RegisterMethod(b.file, "writable", methods::File_writable);
			RegisterMethod(b.file, "seekable", methods::File_seekable);
			RegisterMethod(b.file, "seek", methods::File_seek);
			RegisterMethod(b.file, "tell", methods::File_tell);
			if (context->config.enableOSAccess)
				Wg_SetGlobal(context, "open", b.file);

			// Add native free functions
			b.isinstance = RegisterFunction(context, "isinstance", lib::isinstance);
			RegisterFunction(context, "bin", lib::base_str<2>);
			RegisterFunction(context, "oct", lib::base_str<8>);
			RegisterFunction(context, "hex", lib::base_str<16>);
			RegisterFunction(context, "callable", lib::callable);
			RegisterFunction(context, "chr", lib::chr);
			RegisterFunction(context, "compile", lib::compile);
			RegisterFunction(context, "eval", lib::eval);
			RegisterFunction(context, "exec", lib::exec);
			RegisterFunction(context, "getattr", lib::getattr);
			RegisterFunction(context, "id", lib::id);
			RegisterFunction(context, "input", lib::input);
			RegisterFunction(context, "ord", lib::ord);
			RegisterFunction(context, "pow", lib::pow);
			RegisterFunction(context, "print", lib::print);
			RegisterFunction(context, "round", lib::round);
			RegisterFunction(context, "setattr", lib::setattr);
			if (context->config.isatty) {
				RegisterFunction(context, "exit", lib::exit);
				RegisterFunction(context, "quit", lib::exit);
			}
			
			// Initialize the rest with a script
			if (Execute(context, CODE, "__builtins__") == nullptr)
				throw LibraryInitException();

			b.len = getGlobal("len");
			b.repr = getGlobal("repr");
			b.hash = getGlobal("hash");
			b.slice = getGlobal("slice");
			b.defaultIter = getGlobal("__DefaultIter");
			b.defaultReverseIter = getGlobal("__DefaultReverseIter");
			b.codeObject = getGlobal("__CodeObject");
			b.moduleObject = createClass("ModuleObject", nullptr, false);
			b.readlineIter = getGlobal("__ReadLineIter");
			
			b.baseException = getGlobal("BaseException");
			b.systemExit = getGlobal("SystemExit");
			b.exception = getGlobal("Exception");
			b.stopIteration = getGlobal("StopIteration");
			b.arithmeticError = getGlobal("ArithmeticError");
			b.overflowError = getGlobal("OverflowError");
			b.zeroDivisionError = getGlobal("ZeroDivisionError");
			b.attributeError = getGlobal("AttributeError");
			b.importError = getGlobal("ImportError");
			b.syntaxError = getGlobal("SyntaxError");
			b.lookupError = getGlobal("LookupError");
			b.indexError = getGlobal("IndexError");
			b.keyError = getGlobal("KeyError");
			b.memoryError = getGlobal("MemoryError");
			b.nameError = getGlobal("NameError");
			b.osError = getGlobal("OSError");
			b.isADirectoryError = getGlobal("IsADirectoryError");
			b.runtimeError = getGlobal("RuntimeError");
			b.notImplementedError = getGlobal("NotImplementedError");
			b.recursionError = getGlobal("RecursionError");
			b.typeError = getGlobal("TypeError");
			b.valueError = getGlobal("ValueError");

			b.memoryErrorInstance = Wg_Call(b.memoryError, nullptr, 0);
			if (b.memoryErrorInstance == nullptr)
				throw LibraryInitException();
			
		} catch (LibraryInitException&) {
			std::abort(); // Internal error
		}
		return true;
	}
} // namespace wings
