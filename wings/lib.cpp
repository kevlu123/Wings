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

static bool ParseIndex(Wg_Obj* container, Wg_Obj* index, Wg_int& out, std::optional<Wg_int>& size) {
	Wg_Obj* len = Wg_UnaryOp(WG_UOP_LEN, container);
	if (len == nullptr)
		return false;

	if (!Wg_IsInt(index)) {
		Wg_RaiseTypeError(container->context, "index must be an integer");
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

static bool ParseIndex(Wg_Obj* container, Wg_Obj* index, Wg_int& out) {
	std::optional<Wg_int> size;
	return ParseIndex(container, index, out, size);
}

template <class F>
static bool IterateRange(Wg_int start, Wg_int stop, Wg_int step, F f) {
	WASSERT(step);
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

static bool ParseSlice(Wg_Obj* container, Wg_Obj* slice, Wg_int& start, Wg_int& stop, Wg_int& step) {
	std::optional<Wg_int> size;
	std::vector<wings::WObjRef> refs;
	refs.emplace_back(container);
	refs.emplace_back(slice);

	Wg_Obj* stepAttr = Wg_GetAttribute(slice, "step");
	refs.emplace_back(stepAttr);
	if (stepAttr == nullptr) {
		return false;
	} else if (Wg_IsNone(stepAttr)) {
		step = 1;
	} else if (!Wg_IsInt(stepAttr)) {
		Wg_RaiseTypeError(slice->context, "slice step attribute must be an integer");
		return false;
	} else if ((step = Wg_GetInt(stepAttr)) == 0) {
		Wg_RaiseValueError(slice->context, "slice step cannot be 0");
		return false;
	}

	Wg_Obj* startAttr = Wg_GetAttribute(slice, "start");
	refs.emplace_back(startAttr);
	bool hasStart = true;
	if (startAttr == nullptr) {
		return false;
	} else if (Wg_IsNone(startAttr)) {
		hasStart = false;
	} else if (!ParseIndex(container, startAttr, start, size)) {
		return false;
	}

	Wg_Obj* stopAttr = Wg_GetAttribute(slice, "stop");
	refs.emplace_back(stopAttr);
	bool hasStop = true;
	if (stopAttr == nullptr) {
		return false;
	} else if (Wg_IsNone(stopAttr)) {
		hasStop = false;
	} else if (!ParseIndex(container, stopAttr, stop, size)) {
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

#define EXPECT_ARG_COUNT(n) do if (argc != n) { Wg_RaiseArgumentCountError(context, argc, n); return nullptr; } while (0)
#define EXPECT_ARG_COUNT_AT_LEAST(n) do if (argc < n) { Wg_RaiseArgumentCountError(context, argc, n); return nullptr; } while (0)
#define EXPECT_ARG_COUNT_BETWEEN(min, max) do if (argc < min || argc > max) { Wg_RaiseArgumentCountError(context, argc, -1); return nullptr; } while (0)
#define EXPECT_ARG_TYPE(index, check, expect) do if (!(check)(argv[index])) { Wg_RaiseArgumentTypeError(context, index, expect); return nullptr; } while (0)
#define EXPECT_ARG_TYPE_NULL(index) EXPECT_ARG_TYPE(index, Wg_IsNone, "NoneType");
#define EXPECT_ARG_TYPE_BOOL(index) EXPECT_ARG_TYPE(index, Wg_IsBool, "bool");
#define EXPECT_ARG_TYPE_INT(index) EXPECT_ARG_TYPE(index, Wg_IsInt, "int");
#define EXPECT_ARG_TYPE_FLOAT(index) EXPECT_ARG_TYPE(index, [](const Wg_Obj* v) { return Wg_IsIntOrFloat(v) && !Wg_IsInt(v); }, "int or float");
#define EXPECT_ARG_TYPE_INT_OR_FLOAT(index) EXPECT_ARG_TYPE(index, Wg_IsIntOrFloat, "int or float");
#define EXPECT_ARG_TYPE_STRING(index) EXPECT_ARG_TYPE(index, Wg_IsString, "str");
#define EXPECT_ARG_TYPE_LIST(index) EXPECT_ARG_TYPE(index, Wg_IsList, "list");
#define EXPECT_ARG_TYPE_TUPLE(index) EXPECT_ARG_TYPE(index, Wg_IsTuple, "tuple");
#define EXPECT_ARG_TYPE_MAP(index) EXPECT_ARG_TYPE(index, Wg_IsDictionary, "dict");
#define EXPECT_ARG_TYPE_SET(index) EXPECT_ARG_TYPE(index, Wg_IsSet, "set");
#define EXPECT_ARG_TYPE_FUNC(index) EXPECT_ARG_TYPE(index, Wg_IsFunction, "function");

namespace wings {

	namespace ctors {

		static Wg_Obj* object(Wg_Context* context, Wg_Obj** argv, int argc) { // Excludes self
			EXPECT_ARG_COUNT(0);

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
			EXPECT_ARG_COUNT_BETWEEN(0, 1);

			if (argc == 1) {
				Wg_Obj* res = Wg_CallMethod(argv[0], "__nonzero__", nullptr, 0);
				if (res == nullptr) {
					return nullptr;
				} else if (!Wg_IsBool(res)) {
					Wg_RaiseTypeError(context, "__nonzero__() returned a non bool type");
					return nullptr;
				}
				return res;
			}

			return context->builtins._false;
		}
		
		static Wg_Obj* _int(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT_BETWEEN(1, 3);

			Wg_int v = 0;
			if (argc >= 2) {
				Wg_Obj* res = Wg_CallMethod(argv[1], "__int__", argv + 2, argc - 2);
				if (res == nullptr) {
					return nullptr;
				} else if (!Wg_IsInt(res)) {
					Wg_RaiseTypeError(context, "__int__() returned a non int type");
					return nullptr;
				}
				v = Wg_GetInt(res);
			}

			argv[0]->attributes = context->builtins._int->Get<Wg_Obj::Class>().instanceAttributes.Copy();
			argv[0]->type = "__int";
			argv[0]->data = new Wg_int(v);
			argv[0]->finalizer.fptr = [](Wg_Obj* obj, void*) { delete (Wg_int*)obj->data; };

			return Wg_CreateNone(context);
		}

		static Wg_Obj* _float(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT_BETWEEN(1, 2);

			Wg_float v = 0;
			if (argc == 2) {
				Wg_Obj* res = Wg_CallMethod(argv[1], "__float__", nullptr, 0);
				if (res == nullptr) {
					return nullptr;
				} else if (!Wg_IsIntOrFloat(res)) {
					Wg_RaiseTypeError(context, "__float__() returned a non float type");
					return nullptr;
				}
				v = Wg_GetFloat(res);
			}

			argv[0]->attributes = context->builtins._float->Get<Wg_Obj::Class>().instanceAttributes.Copy();
			argv[0]->type = "__float";
			argv[0]->data = new Wg_float(v);
			argv[0]->finalizer.fptr = [](Wg_Obj* obj, void*) { delete (Wg_float*)obj->data; };

			return Wg_CreateNone(context);
		}

		static Wg_Obj* str(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT_BETWEEN(1, 2);

			const char* v = "";
			if (argc == 2) {
				Wg_Obj* res = Wg_CallMethod(argv[1], "__str__", nullptr, 0);
				if (res == nullptr) {
					return nullptr;
				} else if (!Wg_IsString(res)) {
					Wg_RaiseTypeError(context, "__str__() returned a non string type");
					return nullptr;
				}
				v = Wg_GetString(res);
			}
			argv[0]->attributes = context->builtins.str->Get<Wg_Obj::Class>().instanceAttributes.Copy();
			argv[0]->type = "__str";
			argv[0]->data = new std::string(v);
			argv[0]->finalizer.fptr = [](Wg_Obj* obj, void*) { delete (std::string*)obj->data; };

			return Wg_CreateNone(context);
		}

		static Wg_Obj* tuple(Wg_Context* context, Wg_Obj** argv, int argc) { // Excludes self
			EXPECT_ARG_COUNT_BETWEEN(0, 1);

			struct State {
				std::vector<Wg_Obj*> v;
				std::vector<WObjRef> refs;
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
			EXPECT_ARG_COUNT_BETWEEN(1, 2);

			struct State {
				std::vector<Wg_Obj*> v;
				std::vector<WObjRef> refs;
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

			return Wg_CreateNone(context);
		}

		static Wg_Obj* map(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT_BETWEEN(1, 2);

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

					Wg_ProtectObject(kv[1]);
					try {
						((WDict*)ud)->operator[](kv[0]) = kv[1];
					} catch (HashException&) {
						Wg_UnprotectObject(kv[1]);
						return false;
					}
					Wg_UnprotectObject(kv[1]);
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

			return Wg_CreateNone(context);
		}

		static Wg_Obj* set(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT_BETWEEN(1, 2);

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
						return true;
					} catch (HashException&) {
						return false;
					}
				};

				if (!Wg_Iterate(iterable, buf, f))
					return nullptr;
			}

			return Wg_CreateNone(context);
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
			EXPECT_ARG_COUNT_BETWEEN(1, 2);
			if (argc == 2) {
				Wg_SetAttribute(argv[0], "_message", argv[1]);
				return Wg_CreateNone(context);
			} else if (Wg_Obj* msg = Wg_CreateString(context)) {
				Wg_SetAttribute(argv[0], "_message", msg);
				return Wg_CreateNone(context);
			} else {
				return nullptr;
			}
		}

		static Wg_Obj* DictIter(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_MAP(1);
			auto* it = new WDict::iterator(argv[1]->Get<WDict>().begin());
			Wg_SetUserdata(argv[0], it);
			argv[0]->finalizer.fptr = [](Wg_Obj* obj, void*) { delete (WDict::iterator*)obj->data; };
			Wg_LinkReference(argv[0], argv[1]);
			return Wg_CreateNone(context);
		}

		static Wg_Obj* SetIter(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_SET(1);
			auto* it = new WSet::iterator(argv[1]->Get<WSet>().begin());
			Wg_SetUserdata(argv[0], it);
			argv[0]->finalizer.fptr = [](Wg_Obj* obj, void*) { delete (WSet::iterator*)obj->data; };
			Wg_LinkReference(argv[0], argv[1]);
			return Wg_CreateNone(context);
		}
		
	} // namespace ctors

	namespace methods {

		static Wg_Obj* object_pos(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			return argv[0];
		}

		static Wg_Obj* object_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			std::string s = "<" + WObjTypeToString(argv[0]) + " object at 0x" + PtrToString(argv[0]) + ">";
			return Wg_CreateString(context, s.c_str());
		}

		static Wg_Obj* object_nonzero(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			return Wg_CreateBool(context, true);
		}

		static Wg_Obj* object_repr(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			return Wg_UnaryOp(WG_UOP_STR, argv[0]);
		}

		static Wg_Obj* object_eq(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			return Wg_CreateBool(context, argv[0] == argv[1]);
		}

		static Wg_Obj* object_ne(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			Wg_Obj* eq = Wg_BinaryOp(WG_BOP_EQ, argv[0], argv[1]);
			if (eq == nullptr)
				return nullptr;
			return Wg_CreateBool(context, !Wg_GetBool(eq));
		}

		static Wg_Obj* object_le(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			Wg_Obj* lt = Wg_BinaryOp(WG_BOP_LT, argv[0], argv[1]);
			if (lt == nullptr)
				return nullptr;
			if (Wg_GetBool(lt))
				return Wg_CreateBool(context, true);			
			return Wg_BinaryOp(WG_BOP_EQ, argv[0], argv[1]);
		}

		static Wg_Obj* object_ge(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			Wg_Obj* lt = Wg_BinaryOp(WG_BOP_LT, argv[0], argv[1]);
			if (lt == nullptr)
				return nullptr;
			return Wg_CreateBool(context, !Wg_GetBool(lt));
		}

		static Wg_Obj* object_gt(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			Wg_Obj* lt = Wg_BinaryOp(WG_BOP_LT, argv[0], argv[1]);
			if (lt == nullptr)
				return nullptr;
			if (Wg_GetBool(lt))
				return Wg_CreateBool(context, false);

			Wg_Obj* eq = Wg_BinaryOp(WG_BOP_EQ, argv[0], argv[1]);
			if (eq == nullptr)
				return nullptr;
			return Wg_CreateBool(context, !Wg_GetBool(eq));
		}

		static Wg_Obj* object_hash(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			Wg_int hash = (Wg_int)std::hash<Wg_Obj*>()(argv[0]);
			return Wg_CreateInt(context, hash);
		}

		static Wg_Obj* object_iadd(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__add__", &argv[1], 1);
		}

		static Wg_Obj* object_isub(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__sub__", &argv[1], 1);
		}

		static Wg_Obj* object_imul(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__mul__", &argv[1], 1);
		}

		static Wg_Obj* object_itruediv(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__truediv__", &argv[1], 1);
		}

		static Wg_Obj* object_ifloordiv(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__floordiv__", &argv[1], 1);
		}

		static Wg_Obj* object_imod(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__mod__", &argv[1], 1);
		}

		static Wg_Obj* object_ipow(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__pow__", &argv[1], 1);
		}

		static Wg_Obj* object_iand(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__and__", &argv[1], 1);
		}

		static Wg_Obj* object_ior(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__or__", &argv[1], 1);
		}

		static Wg_Obj* object_ixor(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__xor__", &argv[1], 1);
		}

		static Wg_Obj* object_ilshift(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__lshift__", &argv[1], 1);
		}

		static Wg_Obj* object_irshift(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			return Wg_CallMethod(argv[0], "__rshift__", &argv[1], 1);
		}

		static Wg_Obj* object_iter(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			return Wg_Call(context->builtins.defaultIter, argv, 1);
		}

		static Wg_Obj* object_reversed(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			return Wg_Call(context->builtins.defaultReverseIter, argv, 1);
		}

		static Wg_Obj* null_nonzero(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_NULL(0);
			return Wg_CreateNone(context);
		}

		static Wg_Obj* null_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_NULL(0);
			return Wg_CreateString(context, "None");
		}

		static Wg_Obj* bool_nonzero(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_BOOL(0);
			return argv[0];
		}

		static Wg_Obj* bool_int(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_BOOL(0);
			return Wg_CreateInt(context, Wg_GetBool(argv[0]) ? 1 : 0);
		}

		static Wg_Obj* bool_float(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_BOOL(0);
			return Wg_CreateFloat(context, Wg_GetBool(argv[0]) ? (Wg_float)1 : (Wg_float)0);
		}

		static Wg_Obj* bool_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_BOOL(0);
			return Wg_CreateString(context, Wg_GetBool(argv[0]) ? "True" : "False");
		}

		static Wg_Obj* bool_eq(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_BOOL(0);
			return Wg_CreateBool(context, Wg_IsBool(argv[1]) && Wg_GetBool(argv[0]) == Wg_GetBool(argv[1]));
		}

		static Wg_Obj* bool_hash(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_BOOL(0);
			Wg_int hash = (Wg_int)std::hash<bool>()(Wg_GetBool(argv[0]));
			return Wg_CreateInt(context, hash);
		}

		static Wg_Obj* bool_abs(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_BOOL(0);
			return Wg_CreateInt(context, Wg_GetBool(argv[0]) ? 1 : 0);
		}

		static Wg_Obj* int_nonzero(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_INT(0);
			return Wg_CreateBool(context, Wg_GetInt(argv[0]) != 0);
		}

		static Wg_Obj* int_int(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_INT(0);
			return argv[0];
		}

		static Wg_Obj* int_float(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_INT(0);
			return Wg_CreateFloat(context, Wg_GetFloat(argv[0]));
		}

		static Wg_Obj* int_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_INT(0);
			return Wg_CreateString(context, std::to_string(argv[0]->Get<Wg_int>()).c_str());
		}

		static Wg_Obj* int_eq(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);
			return Wg_CreateBool(context, Wg_IsInt(argv[1]) && Wg_GetInt(argv[0]) == Wg_GetInt(argv[1]));
		}

		static Wg_Obj* int_lt(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return Wg_CreateBool(context, Wg_GetFloat(argv[0]) < Wg_GetFloat(argv[1]));
		}

		static Wg_Obj* int_hash(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_INT(0);
			//Wg_int hash = (Wg_int)std::hash<Wg_int>()(Wg_GetInt(argv[0]));
			Wg_int hash = Wg_GetInt(argv[0]);
			return Wg_CreateInt(context, hash);
		}

		static Wg_Obj* int_abs(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_INT(0);
			return Wg_CreateInt(context, std::abs(Wg_GetInt(argv[0])));
		}

		static Wg_Obj* int_neg(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_INT(0);
			return Wg_CreateInt(context, -Wg_GetInt(argv[0]));
		}

		static Wg_Obj* int_add(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			if (Wg_IsInt(argv[1])) {
				return Wg_CreateInt(context, Wg_GetInt(argv[0]) + Wg_GetInt(argv[1]));
			} else {
				return Wg_CreateFloat(context, Wg_GetFloat(argv[0]) + Wg_GetFloat(argv[1]));
			}
		}

		static Wg_Obj* int_sub(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			if (Wg_IsInt(argv[1])) {
				return Wg_CreateInt(context, Wg_GetInt(argv[0]) - Wg_GetInt(argv[1]));
			} else {
				return Wg_CreateFloat(context, Wg_GetFloat(argv[0]) - Wg_GetFloat(argv[1]));
			}
		}

		static Wg_Obj* int_mul(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);

			if (Wg_IsString(argv[1])) {
				Wg_int multiplier = Wg_GetInt(argv[0]);
				std::string s;
				for (Wg_int i = 0; i < multiplier; i++)
					s += Wg_GetString(argv[1]);
				return Wg_CreateString(context, s.c_str());
			} else if (Wg_IsInt(argv[1])) {
				return Wg_CreateInt(context, Wg_GetInt(argv[0]) * Wg_GetInt(argv[1]));
			} else if (Wg_IsIntOrFloat(argv[1])) {
				return Wg_CreateFloat(context, Wg_GetFloat(argv[0]) * Wg_GetFloat(argv[1]));
			} else {
				EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
				return nullptr;
			}
		}

		static Wg_Obj* int_truediv(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(0);

			if (Wg_GetFloat(argv[1]) == 0) {
				Wg_RaiseZeroDivisionError(context);
				return nullptr;
			}
			return Wg_CreateFloat(context, Wg_GetFloat(argv[0]) / Wg_GetFloat(argv[1]));
		}

		static Wg_Obj* int_floordiv(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);

			if (Wg_GetFloat(argv[1]) == 0) {
				Wg_RaiseZeroDivisionError(context);
				return nullptr;
			}

			if (Wg_IsInt(argv[1])) {
				return Wg_CreateInt(context, (Wg_int)std::floor(Wg_GetFloat(argv[0]) / Wg_GetFloat(argv[1])));
			} else {
				return Wg_CreateFloat(context, std::floor(Wg_GetFloat(argv[0]) / Wg_GetFloat(argv[1])));
			}
		}

		static Wg_Obj* int_mod(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);

			if (Wg_GetFloat(argv[1]) == 0) {
				Wg_RaiseZeroDivisionError(context);
				return nullptr;
			}

			if (Wg_IsInt(argv[1])) {
				Wg_int mod = Wg_GetInt(argv[1]);
				Wg_int m = Wg_GetInt(argv[0]) % mod;
				if (m < 0)
					m += mod;
				return Wg_CreateInt(context, m);
			} else {
				return Wg_CreateFloat(context, std::fmod(Wg_GetFloat(argv[0]), Wg_GetFloat(argv[1])));
			}
		}

		static Wg_Obj* int_pow(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return Wg_CreateFloat(context, std::pow(Wg_GetFloat(argv[0]), Wg_GetFloat(argv[1])));
		}

		static Wg_Obj* int_and(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);
			EXPECT_ARG_TYPE_INT(1);
			return Wg_CreateInt(context, Wg_GetInt(argv[0]) & Wg_GetInt(argv[1]));
		}

		static Wg_Obj* int_or(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);
			EXPECT_ARG_TYPE_INT(1);
			return Wg_CreateInt(context, Wg_GetInt(argv[0]) | Wg_GetInt(argv[1]));
		}

		static Wg_Obj* int_xor(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);
			EXPECT_ARG_TYPE_INT(1);
			return Wg_CreateInt(context, Wg_GetInt(argv[0]) ^ Wg_GetInt(argv[1]));
		}

		static Wg_Obj* int_invert(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_INT(0);
			return Wg_CreateInt(context, ~Wg_GetInt(argv[0]));
		}

		static Wg_Obj* int_lshift(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);
			EXPECT_ARG_TYPE_INT(1);

			Wg_int shift = Wg_GetInt(argv[1]);
			if (shift < 0) {
				Wg_RaiseValueError(context, "Shift cannot be negative");
				return nullptr;
			}
			shift = std::min(shift, (Wg_int)sizeof(Wg_int) * 8);
			return Wg_CreateInt(context, Wg_GetInt(argv[0]) << shift);
		}

		static Wg_Obj* int_rshift(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT(0);
			EXPECT_ARG_TYPE_INT(1);

			Wg_int shift = Wg_GetInt(argv[1]);
			if (shift < 0) {
				Wg_RaiseValueError(context, "Shift cannot be negative");
				return nullptr;
			}
			shift = std::min(shift, (Wg_int)sizeof(Wg_int) * 8);
			return Wg_CreateInt(context, Wg_GetInt(argv[0]) >> shift);
		}

		static Wg_Obj* int_bit_length(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_INT(0);

			wuint n = (wuint)Wg_GetInt(argv[0]);
			return Wg_CreateInt(context, (Wg_int)std::bit_width(n));
		}

		static Wg_Obj* int_bit_count(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_INT(0);

			wuint n = (wuint)Wg_GetInt(argv[0]);
			return Wg_CreateInt(context, (Wg_int)std::popcount(n));
		}

		static Wg_Obj* float_nonzero(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			return Wg_CreateBool(context, Wg_GetFloat(argv[0]) != 0);
		}

		static Wg_Obj* float_int(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			return Wg_CreateInt(context, (Wg_int)Wg_GetFloat(argv[0]));
		}

		static Wg_Obj* float_float(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			return argv[0];
		}

		static Wg_Obj* float_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_FLOAT(0);
			std::string s = std::to_string(argv[0]->Get<Wg_float>());
			s.erase(s.find_last_not_of('0') + 1, std::string::npos);
			if (s.ends_with('.'))
				s.push_back('0');
			return Wg_CreateString(context, s.c_str());
		}

		static Wg_Obj* float_eq(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			return Wg_CreateBool(context, Wg_IsIntOrFloat(argv[1]) && Wg_GetFloat(argv[0]) == Wg_GetFloat(argv[1]));
		}

		static Wg_Obj* float_lt(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return Wg_CreateBool(context, Wg_GetFloat(argv[0]) < Wg_GetFloat(argv[1]));
		}

		static Wg_Obj* float_hash(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_FLOAT(0);
			Wg_int hash = (Wg_int)std::hash<Wg_float>()(Wg_GetFloat(argv[0]));
			return Wg_CreateInt(context, hash);
		}

		static Wg_Obj* float_abs(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_FLOAT(0);
			return Wg_CreateFloat(context, std::abs(Wg_GetFloat(argv[0])));
		}

		static Wg_Obj* float_neg(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			return Wg_CreateFloat(context, -Wg_GetFloat(argv[0]));
		}

		static Wg_Obj* float_add(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return Wg_CreateFloat(context, Wg_GetFloat(argv[0]) + Wg_GetFloat(argv[1]));
		}

		static Wg_Obj* float_sub(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return Wg_CreateFloat(context, Wg_GetFloat(argv[0]) - Wg_GetFloat(argv[1]));
		}

		static Wg_Obj* float_mul(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return Wg_CreateFloat(context, Wg_GetFloat(argv[0]) * Wg_GetFloat(argv[1]));
		}

		static Wg_Obj* float_truediv(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return Wg_CreateFloat(context, Wg_GetFloat(argv[0]) / Wg_GetFloat(argv[1]));
		}

		static Wg_Obj* float_floordiv(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return Wg_CreateFloat(context, std::floor(Wg_GetFloat(argv[0]) / Wg_GetFloat(argv[1])));
		}

		static Wg_Obj* float_mod(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return Wg_CreateFloat(context, std::fmod(Wg_GetFloat(argv[0]), Wg_GetFloat(argv[1])));
		}

		static Wg_Obj* float_pow(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
			EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			return Wg_CreateFloat(context, std::pow(Wg_GetFloat(argv[0]), Wg_GetFloat(argv[1])));
		}

		static Wg_Obj* float_is_integer(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_FLOAT(0);

			Wg_float f = Wg_GetFloat(argv[0]);
			return Wg_CreateBool(context, std::floor(f) == f);
		}

		static Wg_Obj* str_nonzero(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_STRING(0);
			std::string s = Wg_GetString(argv[0]);
			return Wg_CreateBool(context, !s.empty());
		}

		static Wg_Obj* str_int(Wg_Context* context, Wg_Obj** argv, int argc) {
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
						switch (base) {
						case 2: Wg_RaiseValueError(context, "Invalid binary string"); return nullptr;
						case 8: Wg_RaiseValueError(context, "Invalid octal string"); return nullptr;
						case 16: Wg_RaiseValueError(context, "Invalid hexadecimal string"); return nullptr;
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
				Wg_RaiseException(context, "Integer string is too large", context->builtins.overflowError);
				return nullptr;
			}

			if (*p) {
				Wg_RaiseValueError(context, "Invalid integer string");
				return nullptr;
			}

			return Wg_CreateInt(context, (Wg_int)value);
		}

		static Wg_Obj* str_float(Wg_Context* context, Wg_Obj** argv, int argc) {
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

			std::string s = Wg_GetString(argv[0]);
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
					case 2: Wg_RaiseValueError(context, "Invalid binary string"); return nullptr;
					case 8: Wg_RaiseValueError(context, "Invalid octal string"); return nullptr;
					case 16: Wg_RaiseValueError(context, "Invalid hexadecimal string"); return nullptr;
					default: WUNREACHABLE();
					}
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
				Wg_RaiseValueError(context, "Invalid float string");
				return nullptr;
			}

			return Wg_CreateFloat(context, fvalue);
		}

		static Wg_Obj* str_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_STRING(0);
			return argv[0];
		}

		static Wg_Obj* str_repr(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_STRING(0);

			std::string s = Wg_GetString(argv[0]);
			return Wg_CreateString(context, ("'" + s + "'").c_str());
		}

		static Wg_Obj* str_len(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_STRING(0);
			return Wg_CreateInt(context, (Wg_int)argv[0]->Get<std::string>().size());
		}

		static Wg_Obj* str_eq(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_STRING(0);
			return Wg_CreateBool(context, Wg_IsString(argv[1]) && std::strcmp(Wg_GetString(argv[0]), Wg_GetString(argv[1])) == 0);
		}

		static Wg_Obj* str_lt(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_STRING(0);
			EXPECT_ARG_TYPE_STRING(1);
			return Wg_CreateBool(context, std::strcmp(Wg_GetString(argv[0]), Wg_GetString(argv[1])) < 0);
		}

		static Wg_Obj* str_hash(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_STRING(0);
			Wg_int hash = (Wg_int)std::hash<std::string_view>()(Wg_GetString(argv[0]));
			return Wg_CreateInt(context, hash);
		}

		static Wg_Obj* str_add(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_STRING(0);
			EXPECT_ARG_TYPE_STRING(1);
			std::string s = Wg_GetString(argv[0]);
			s += Wg_GetString(argv[1]);
			return Wg_CreateString(context, s.c_str());
		}

		static Wg_Obj* str_mul(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_STRING(0);
			EXPECT_ARG_TYPE_INT(1);
			Wg_int multiplier = Wg_GetInt(argv[1]);
			std::string_view arg = Wg_GetString(argv[0]);
			std::string s;
			s.reserve(arg.size() * (size_t)multiplier);
			for (Wg_int i = 0; i < multiplier; i++)
				s += arg;
			return Wg_CreateString(context, s.c_str());
		}

		static Wg_Obj* str_contains(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_STRING(0);
			EXPECT_ARG_TYPE_STRING(1);
			return Wg_CreateBool(context, std::strstr(Wg_GetString(argv[0]), Wg_GetString(argv[1])));
		}

		static Wg_Obj* str_getitem(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_STRING(0);

			if (Wg_IsInt(argv[1])) {
				Wg_int index;
				if (!ParseIndex(argv[0], argv[1], index))
					return nullptr;

				std::string_view s = Wg_GetString(argv[0]);
				if (index < 0 || index >= (Wg_int)s.size()) {
					Wg_RaiseIndexError(context);
					return nullptr;
				}

				char buf[2] = { s[index], '\0' };
				return Wg_CreateString(context, buf);
			} else if (Wg_IsInstance(argv[1], &context->builtins.slice, 1)) {
				Wg_int start, stop, step;
				if (!ParseSlice(argv[0], argv[1], start, stop, step))
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
				
				return Wg_CreateString(context, sliced.c_str());
			} else {
				Wg_RaiseArgumentTypeError(context, 1, "int or slice");
				return nullptr;
			}
		}

		static Wg_Obj* str_capitalize(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_STRING(0);
			std::string s = Wg_GetString(argv[0]);
			if (!s.empty())
				s[0] = (char)std::toupper(s[0]);
			return Wg_CreateString(context, s.c_str());
		}

		static Wg_Obj* str_lower(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_STRING(0);

			std::string s = Wg_GetString(argv[0]);
			std::transform(s.begin(), s.end(), s.begin(), std::tolower);
			return Wg_CreateString(context, s.c_str());
		}

		static Wg_Obj* str_upper(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_STRING(0);

			std::string s = Wg_GetString(argv[0]);
			std::transform(s.begin(), s.end(), s.begin(), std::toupper);
			return Wg_CreateString(context, s.c_str());
		}

		static Wg_Obj* str_casefold(Wg_Context* context, Wg_Obj** argv, int argc) {
			return str_lower(context, argv, argc);
		}

		static Wg_Obj* str_center(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT_BETWEEN(2, 3);
			EXPECT_ARG_TYPE_STRING(0);
			EXPECT_ARG_TYPE_INT(1);
			if (argc >= 3) EXPECT_ARG_TYPE_STRING(2);
			
			const char* fill = argc == 3 ? Wg_GetString(argv[2]) : " ";
			if (std::strlen(fill) != 1) {
				Wg_RaiseTypeError(context, "The fill character must be exactly one character long");
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

			return Wg_CreateString(context, s.c_str());
		}

		static Wg_Obj* str_count(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_STRING(0);
			EXPECT_ARG_TYPE_STRING(1);
			
			std::string_view s = Wg_GetString(argv[0]);
			std::string_view search = Wg_GetString(argv[1]);			
			Wg_int count = 0;
			size_t pos = 0;
			while ((pos = s.find(search, pos)) != std::string_view::npos) {
				count++;
				pos += search.size();
			}

			return Wg_CreateInt(context, count);
		}

		static Wg_Obj* str_format(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT_AT_LEAST(1);
			EXPECT_ARG_TYPE_STRING(0);
			
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
						Wg_RaiseValueError(context, "Invalid format string");
						return nullptr;
					}
				}

				if (useAutoIndexing) {
					if (mode == Mode::Manual) {
						Wg_RaiseValueError(
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
						Wg_RaiseValueError(
							context,
							"cannot switch from automatic field numbering to manual field specification"
						);
						return nullptr;
					}
					mode = Mode::Manual;
				}

				if ((int)index >= argc - 1) {
					Wg_RaiseIndexError(context);
					return nullptr;
				}

				Wg_Obj* item = Wg_UnaryOp(WG_UOP_STR, argv[index + 1]);
				if (item == nullptr)
					return nullptr;
				s += Wg_GetString(item);
			}

			return Wg_CreateString(context, s.c_str());
		}

		static Wg_Obj* str_startswith(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_STRING(0);
			EXPECT_ARG_TYPE_STRING(1);

			std::string_view s = Wg_GetString(argv[0]);
			std::string_view end = Wg_GetString(argv[1]);
			return Wg_CreateBool(context, s.starts_with(end));
		}

		static Wg_Obj* str_endswith(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_STRING(0);
			EXPECT_ARG_TYPE_STRING(1);

			std::string_view s = Wg_GetString(argv[0]);
			std::string_view end = Wg_GetString(argv[1]);
			return Wg_CreateBool(context, s.ends_with(end));
		}

		template <bool reverse>
		static Wg_Obj* str_findx(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT_BETWEEN(2, 4);
			EXPECT_ARG_TYPE_STRING(0);
			EXPECT_ARG_TYPE_STRING(1);
			
			Wg_int start = 0;
			std::optional<Wg_int> size;
			if (argc >= 3) {
				EXPECT_ARG_TYPE_INT(2);
				if (!ParseIndex(argv[0], argv[2], start, size))
					return nullptr;
			}

			Wg_int end = 0;
			if (argc >= 4) {
				EXPECT_ARG_TYPE_INT(3);
				if (!ParseIndex(argv[0], argv[3], end, size))
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
				return Wg_CreateInt(context, -1);
			} else {
				return Wg_CreateInt(context, (Wg_int)location);
			}
		}

		template <bool reverse>
		static Wg_Obj* str_indexx(Wg_Context* context, Wg_Obj** argv, int argc) {
			Wg_Obj* location = str_findx<reverse>(context, argv, argc);
			if (location == nullptr)
				return nullptr;
			
			if (Wg_GetInt(location) == -1) {
				Wg_RaiseValueError(context, "substring not found");
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
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_STRING(0);

			std::string_view s = Wg_GetString(argv[0]);
			return Wg_CreateBool(context, std::all_of(s.begin(), s.end(), F));
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
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_STRING(0);

			std::string_view s = Wg_GetString(argv[0]);
			constexpr auto f = [](char c) { return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9') || c == '_'; };
			bool allAlphaNum = std::all_of(s.begin(), s.end(), f);
			return Wg_CreateBool(context, allAlphaNum && (s.empty() || s[0] < '0' || s[0] > '9'));
		}

		static Wg_Obj* str_join(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_STRING(0);

			struct State {
				std::string_view sep;
				std::string s;
			} state = { Wg_GetString(argv[0]) };

			bool success = Wg_Iterate(argv[1], &state, [](Wg_Obj* obj, void* ud) {
				State& state = *(State*)ud;
				Wg_Context* context = obj->context;

				if (!Wg_IsString(obj)) {
					Wg_RaiseTypeError(context, "sequence item must be a string");
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

			return Wg_CreateString(context, state.s.c_str());
		}

		static Wg_Obj* str_replace(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT_BETWEEN(3, 4);
			EXPECT_ARG_TYPE_STRING(0);
			EXPECT_ARG_TYPE_STRING(1);
			EXPECT_ARG_TYPE_STRING(2);

			Wg_int count = std::numeric_limits<Wg_int>::max();
			if (argc == 4) {
				EXPECT_ARG_TYPE_INT(3);
				count = Wg_GetInt(argv[3]);
			}

			std::string s = Wg_GetString(argv[0]);
			std::string_view find = Wg_GetString(argv[1]);
			std::string_view repl = Wg_GetString(argv[2]);
			StringReplace(s, find, repl, count);
			return Wg_CreateString(context, s.c_str());
		}

		template <bool left, bool zfill = false>
		static Wg_Obj* str_just(Wg_Context* context, Wg_Obj** argv, int argc) {
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
					std::string_view fillStr = Wg_GetString(argv[2]);
					if (fillStr.size() != 1) {
						Wg_RaiseTypeError(context, "The fill character must be exactly one character long");
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
			return Wg_CreateString(context, s.c_str());
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
			EXPECT_ARG_COUNT_BETWEEN(1, 2);
			EXPECT_ARG_TYPE_STRING(0);

			std::string_view chars = " ";
			if (argc == 2 && !Wg_IsNone(argv[1])) {
				EXPECT_ARG_TYPE_STRING(1);
				chars = Wg_GetString(argv[1]);
			}

			std::string_view s = Wg_GetString(argv[0]);
			size_t pos = s.find_first_not_of(chars);
			if (pos == std::string::npos)
				return Wg_CreateString(context);
			return Wg_CreateString(context, s.data() + pos);
		}

		static Wg_Obj* str_rstrip(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT_BETWEEN(1, 2);
			EXPECT_ARG_TYPE_STRING(0);

			std::string_view chars = " ";
			if (argc == 2 && !Wg_IsNone(argv[1])) {
				EXPECT_ARG_TYPE_STRING(1);
				chars = Wg_GetString(argv[1]);
			}

			std::string s = Wg_GetString(argv[0]);
			size_t pos = s.find_last_not_of(chars);
			if (pos == std::string::npos)
				return Wg_CreateString(context);
			s.erase(s.begin() + pos + 1, s.end());
			return Wg_CreateString(context, s.c_str());
		}

		static Wg_Obj* str_strip(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT_BETWEEN(1, 2);
			EXPECT_ARG_TYPE_STRING(0);

			std::string_view chars = " ";
			if (argc == 2 && !Wg_IsNone(argv[1])) {
				EXPECT_ARG_TYPE_STRING(1);
				chars = Wg_GetString(argv[1]);
			}

			std::string s = Wg_GetString(argv[0]);
			size_t pos = s.find_last_not_of(chars);
			if (pos == std::string::npos)
				return Wg_CreateString(context);
			s.erase(s.begin() + pos + 1, s.end());

			pos = s.find_first_not_of(chars);
			if (pos == std::string::npos)
				return Wg_CreateString(context);
			return Wg_CreateString(context, s.data() + pos);
		}

		static Wg_Obj* str_split(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT_BETWEEN(1, 3);
			EXPECT_ARG_TYPE_STRING(0);

			Wg_int maxSplit = -1;
			if (argc == 3) {
				EXPECT_ARG_TYPE_INT(2);
				maxSplit = Wg_GetInt(argv[2]);
			}
			if (maxSplit == -1)
				maxSplit = std::numeric_limits<Wg_int>::max();

			std::vector<std::string> strings;
			if (argc >= 2) {
				EXPECT_ARG_TYPE_STRING(1);
				strings = StringSplit(Wg_GetString(argv[0]), Wg_GetString(argv[1]), maxSplit);
			} else {
				strings = StringSplitChar(Wg_GetString(argv[0]), " \t\n\r\v\f", maxSplit);
			}

			Wg_Obj* li = Wg_CreateList(context);
			if (li == nullptr)
				return nullptr;
			WObjRef ref(li);

			for (const auto& s : strings) {
				Wg_Obj* str = Wg_CreateString(context, s.c_str());
				if (str == nullptr)
					return nullptr;
				li->Get<std::vector<Wg_Obj*>>().push_back(str);
			}
			return li;
		}

		static Wg_Obj* str_splitlines(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT_BETWEEN(1, 2);
			EXPECT_ARG_TYPE_STRING(0);

			bool keepLineBreaks = false;
			if (argc == 2) {
				EXPECT_ARG_TYPE_BOOL(1);
				keepLineBreaks = Wg_GetBool(argv[1]);
			}

			std::vector<std::string> strings = StringSplitLines(Wg_GetString(argv[0]), keepLineBreaks);

			Wg_Obj* li = Wg_CreateList(context);
			if (li == nullptr)
				return nullptr;
			WObjRef ref(li);

			for (const auto& s : strings) {
				Wg_Obj* str = Wg_CreateString(context, s.c_str());
				if (str == nullptr)
					return nullptr;
				li->Get<std::vector<Wg_Obj*>>().push_back(str);
			}
			return li;
		}

		template <Collection collection>
		static Wg_Obj* collection_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			constexpr bool isTuple = collection == Collection::Tuple;
			EXPECT_ARG_COUNT(1);
			if constexpr (isTuple) {
				EXPECT_ARG_TYPE_TUPLE(0);
			} else {
				EXPECT_ARG_TYPE_LIST(0);
			}

			auto it = std::find(context->reprStack.rbegin(), context->reprStack.rend(), argv[0]);
			if (it != context->reprStack.rend()) {
				return Wg_CreateString(context, isTuple ? "(...)" : "[...]");
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
				return Wg_CreateString(context, s.c_str());
			}
		}

		template <Collection collection>
		static Wg_Obj* collection_nonzero(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			if constexpr (collection == Collection::List) {
				EXPECT_ARG_TYPE_LIST(0);
			} else {
				EXPECT_ARG_TYPE_TUPLE(0);
			}

			return Wg_CreateBool(context, !argv[0]->Get<std::vector<Wg_Obj*>>().empty());
		}

		template <Collection collection>
		static Wg_Obj* collection_lt(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			if constexpr (collection == Collection::List) {
				EXPECT_ARG_TYPE_LIST(0);
				EXPECT_ARG_TYPE_LIST(1);
			} else {
				EXPECT_ARG_TYPE_TUPLE(0);
				EXPECT_ARG_TYPE_TUPLE(1);
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
					return Wg_CreateBool(context, false);
			}

			return Wg_CreateBool(context, buf1.size() < buf2.size());
		}

		template <Collection collection>
		static Wg_Obj* collection_eq(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			if constexpr (collection == Collection::List) {
				EXPECT_ARG_TYPE_LIST(0);
				if (!Wg_IsInstance(argv[1], &context->builtins.list, 1))
					return Wg_CreateBool(context, false);
			} else {
				EXPECT_ARG_TYPE_TUPLE(0);
				if (!Wg_IsInstance(argv[1], &context->builtins.tuple, 1))
					return Wg_CreateBool(context, false);
			}

			auto& buf1 = argv[0]->Get<std::vector<Wg_Obj*>>();
			auto& buf2 = argv[1]->Get<std::vector<Wg_Obj*>>();

			if (buf1.size() != buf2.size())
				return Wg_CreateBool(context, false);

			for (size_t i = 0; i < buf1.size(); i++) {
				if (Wg_Obj* eq = Wg_BinaryOp(WG_BOP_EQ, buf1[i], buf2[i])) {
					if (!Wg_GetBool(eq))
						return eq;
				} else {
					return nullptr;
				}
			}

			return Wg_CreateBool(context, true);
		}

		template <Collection collection>
		static Wg_Obj* collection_contains(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			if constexpr (collection == Collection::List) {
				EXPECT_ARG_TYPE_LIST(0);
			} else {
				EXPECT_ARG_TYPE_TUPLE(0);
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

			return Wg_CreateBool(context, false);
		}

		template <Collection collection>
		static Wg_Obj* collection_len(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			if constexpr (collection == Collection::List) {
				EXPECT_ARG_TYPE_LIST(0);
			} else {
				EXPECT_ARG_TYPE_TUPLE(0);
			}

			return Wg_CreateInt(context, (Wg_int)argv[0]->Get<std::vector<Wg_Obj*>>().size());
		}

		template <Collection collection>
		static Wg_Obj* collection_count(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			if constexpr (collection == Collection::List) {
				EXPECT_ARG_TYPE_LIST(0);
			} else {
				EXPECT_ARG_TYPE_TUPLE(0);
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

			return Wg_CreateInt(context, count);
		}

		template <Collection collection>
		static Wg_Obj* collection_index(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			if constexpr (collection == Collection::List) {
				EXPECT_ARG_TYPE_LIST(0);
			} else {
				EXPECT_ARG_TYPE_TUPLE(0);
			}

			auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();
			for (size_t i = 0; i < buf.size(); i++) {
				Wg_Obj* eq = Wg_BinaryOp(WG_BOP_EQ, argv[1], buf[i]);
				if (eq == nullptr)
					return nullptr;
				if (Wg_GetBool(eq))
					return Wg_CreateInt(context, (Wg_int)i);
			}

			Wg_RaiseValueError(context, "Value was not found");
			return nullptr;
		}

		template <Collection collection>
		static Wg_Obj* collection_getitem(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			if constexpr (collection == Collection::List) {
				EXPECT_ARG_TYPE_LIST(0);
			} else {
				EXPECT_ARG_TYPE_TUPLE(0);
			}

			if (Wg_IsInt(argv[1])) {
				Wg_int index;
				if (!ParseIndex(argv[0], argv[1], index))
					return nullptr;

				auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();
				if (index < 0 || index >= (Wg_int)buf.size()) {
					Wg_RaiseIndexError(context);
					return nullptr;
				}

				return buf[index];
			} else if (Wg_IsInstance(argv[1], &context->builtins.slice, 1)) {
				Wg_int start, stop, step;
				if (!ParseSlice(argv[0], argv[1], start, stop, step))
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
					return Wg_CreateList(context, sliced.data(), (int)sliced.size());
				} else {
					return Wg_CreateTuple(context, sliced.data(), (int)sliced.size());
				}
			} else {
				Wg_RaiseArgumentTypeError(context, 1, "int or slice");
				return nullptr;
			}
		}

		static Wg_Obj* list_setitem(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(3);
			EXPECT_ARG_TYPE_LIST(0);
			EXPECT_ARG_TYPE_INT(1);
			
			Wg_int index;
			if (!ParseIndex(argv[0], argv[1], index))
				return nullptr;

			auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();
			if (index < 0 || index >= (Wg_int)buf.size()) {
				Wg_RaiseIndexError(context);
				return nullptr;
			}

			buf[index] = argv[2];
			return Wg_CreateNone(context);
		}
		
		static Wg_Obj* list_append(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_LIST(0);

			argv[0]->Get<std::vector<Wg_Obj*>>().push_back(argv[1]);
			return Wg_CreateNone(context);
		}

		static Wg_Obj* list_insert(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(3);
			EXPECT_ARG_TYPE_LIST(0);
			EXPECT_ARG_TYPE_INT(1);

			Wg_int index;
			if (!ParseIndex(argv[0], argv[1], index))
				return nullptr;

			auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();			
			index = std::clamp(index, (Wg_int)0, (Wg_int)buf.size() + 1);
			buf.insert(buf.begin() + index, argv[2]);
			return Wg_CreateNone(context);
		}

		static Wg_Obj* list_pop(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT_BETWEEN(1, 2);
			EXPECT_ARG_TYPE_LIST(0);

			auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();
			Wg_int index = (Wg_int)buf.size() - 1;
			if (argc == 2) {
				EXPECT_ARG_TYPE_INT(1);
				if (!ParseIndex(argv[0], argv[1], index))
					return nullptr;
			}

			if (index < 0 || index >= (Wg_int)buf.size()) {
				Wg_RaiseIndexError(context);
				return nullptr;
			}
			
			Wg_Obj* popped = buf[index];
			buf.erase(buf.begin() + index);
			return popped;
		}

		static Wg_Obj* list_remove(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_LIST(0);

			auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();
			for (size_t i = 0; i < buf.size(); i++) {
				Wg_Obj* eq = Wg_BinaryOp(WG_BOP_EQ, argv[1], buf[i]);
				if (eq == nullptr)
					return nullptr;
				
				if (Wg_GetBool(eq)) {
					if (i < buf.size())
						buf.erase(buf.begin() + i);
					return Wg_CreateNone(context);
				}
			}

			Wg_RaiseValueError(context, "Value was not found");
			return nullptr;
		}

		static Wg_Obj* list_clear(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_LIST(0);

			argv[0]->Get<std::vector<Wg_Obj*>>().clear();
			return Wg_CreateNone(context);
		}

		static Wg_Obj* list_copy(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_LIST(0);

			auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();
			return Wg_CreateList(context, buf.data(), !buf.size());
		}

		static Wg_Obj* list_extend(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_LIST(0);

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
			
			return Wg_CreateNone(context);
		}

		static Wg_Obj* list_sort(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_LIST(0);

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
			std::vector<WObjRef> refs;
			for (Wg_Obj* v : buf)
				refs.emplace_back(v);

			if (!MergeSort(buf.data(), buf.size(), kw[1]))
				return nullptr;

			if (reverse)
				std::reverse(buf.begin(), buf.end());
			
			argv[0]->Get<std::vector<Wg_Obj*>>() = std::move(buf);

			return Wg_CreateNone(context);
		}

		static Wg_Obj* list_reverse(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_LIST(0);

			auto& buf = argv[0]->Get<std::vector<Wg_Obj*>>();
			std::reverse(buf.begin(), buf.end());
			return Wg_CreateNone(context);
		}

		static Wg_Obj* map_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_MAP(0);

			auto it = std::find(context->reprStack.rbegin(), context->reprStack.rend(), argv[0]);
			if (it != context->reprStack.rend()) {
				return Wg_CreateString(context, "{...}");
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
				return Wg_CreateString(context, (s + "}").c_str());
			}
		}

		static Wg_Obj* map_nonzero(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_MAP(0);
			return Wg_CreateBool(context, !argv[0]->Get<WDict>().empty());
		}

		static Wg_Obj* map_len(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_MAP(0);
			return Wg_CreateInt(context, (Wg_int)argv[0]->Get<WDict>().size());
		}

		static Wg_Obj* map_contains(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_MAP(0);
			try {
				return Wg_CreateBool(context, argv[0]->Get<WDict>().contains(argv[1]));
			} catch (HashException&) {
				return nullptr;
			}
		}

		static Wg_Obj* map_iter(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_MAP(0);
			return Wg_Call(context->builtins.dictKeysIter, argv, 1, nullptr);
		}

		static Wg_Obj* map_values(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_MAP(0);
			return Wg_Call(context->builtins.dictValuesIter, argv, 1, nullptr);
		}

		static Wg_Obj* map_items(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_MAP(0);
			return Wg_Call(context->builtins.dictItemsIter, argv, 1, nullptr);
		}

		static Wg_Obj* map_get(Wg_Context* context, Wg_Obj** argv, int argc) {
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
				return argc == 3 ? argv[2] : Wg_CreateNone(context);
			}

			return it->second;
		}

		static Wg_Obj* map_getitem(Wg_Context* context, Wg_Obj** argv, int argc) {
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
				Wg_RaiseKeyError(context, argv[1]);
				return nullptr;
			}

			return it->second;
		}

		static Wg_Obj* map_setitem(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(3);
			EXPECT_ARG_TYPE_MAP(0);

			try {
				argv[0]->Get<WDict>()[argv[1]] = argv[2];
			} catch (HashException&) {
				return nullptr;
			}
			return Wg_CreateNone(context);
		}

		static Wg_Obj* map_clear(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_MAP(0);
			argv[0]->Get<WDict>().clear();
			return Wg_CreateNone(context);
		}

		static Wg_Obj* map_copy(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_MAP(0);

			std::vector<Wg_Obj*> keys;
			std::vector<Wg_Obj*> values;
			for (const auto& [k, v] : argv[0]->Get<WDict>()) {
				keys.push_back(k);
				values.push_back(v);
			}
			return Wg_CreateDictionary(context, keys.data(), values.data(), (int)keys.size());
		}

		static Wg_Obj* map_pop(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT_BETWEEN(2, 3);
			EXPECT_ARG_TYPE_MAP(0);

			if (auto popped = argv[0]->Get<WDict>().erase(argv[1]))
				return popped.value();

			if (argc == 3)
				return argv[2];

			Wg_RaiseKeyError(context, argv[1]);
			return nullptr;
		}

		static Wg_Obj* map_popitem(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_MAP(0);

			auto& buf = argv[0]->Get<WDict>();
			if (buf.empty()) {
				Wg_RaiseKeyError(context);
				return nullptr;
			}

			auto popped = buf.pop();
			Wg_Obj* tupElems[2] = { popped.first, popped.second };
			return Wg_CreateTuple(context, tupElems, 2);
		}

		static Wg_Obj* map_setdefault(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT_BETWEEN(2, 3);
			EXPECT_ARG_TYPE_MAP(0);

			try {
				auto& entry = argv[0]->Get<WDict>()[argv[1]];
				if (entry == nullptr)
					entry = argc == 3 ? argv[2] : Wg_CreateNone(context);
				return entry;
			} catch (HashException&) {
				return nullptr;
			}
		}

		static Wg_Obj* map_update(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_MAP(0);

			Wg_Obj* iterable = argv[1];
			if (Wg_IsDictionary(argv[1])) {
				iterable = Wg_CallMethod(argv[1], "items", nullptr, 0);
			}

			auto f = [](Wg_Obj* obj, void* ud) {
				Wg_Obj* kv[2]{};
				if (!Wg_Unpack(obj, kv, 2))
					return false;
				
				Wg_ProtectObject(kv[1]);
				try {
					((Wg_Obj*)ud)->Get<WDict>()[kv[0]] = kv[1];
				} catch (HashException&) {
					Wg_UnprotectObject(kv[1]);
					return false;
				}
				Wg_UnprotectObject(kv[1]);
				return true;
			};
			
			if (Wg_Iterate(iterable, argv[0], f)) {
				return Wg_CreateNone(context);
			} else {
				return nullptr;
			}
		}

		static Wg_Obj* set_nonzero(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_SET(0);
			return Wg_CreateBool(context, !argv[0]->Get<WSet>().empty());
		}

		static Wg_Obj* set_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_SET(0);

			auto it = std::find(context->reprStack.rbegin(), context->reprStack.rend(), argv[0]);
			if (it != context->reprStack.rend()) {
				return Wg_CreateString(context, "{...}");
			} else {
				context->reprStack.push_back(argv[0]);
				const auto& buf = argv[0]->Get<wings::WSet>();

				if (buf.empty()) {
					context->reprStack.pop_back();
					return Wg_CreateString(context, "set()");
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
				return Wg_CreateString(context, (s + "}").c_str());
			}
		}

		static Wg_Obj* set_iter(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_SET(0);
			return Wg_Call(context->builtins.setIter, argv, 1, nullptr);
		}

		static Wg_Obj* set_contains(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_SET(0);
			try {
				return Wg_CreateBool(context, argv[0]->Get<WSet>().contains(argv[1]));
			} catch (HashException&) {
				Wg_ClearCurrentException(context);
				return Wg_CreateBool(context, false);
			}
		}

		static Wg_Obj* set_len(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_SET(0);
			return Wg_CreateInt(context, (int)argv[0]->Get<WSet>().size());
		}

		static Wg_Obj* set_clear(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_SET(0);
			argv[0]->Get<WSet>().clear();
			return Wg_CreateNone(context);
		}

		static Wg_Obj* set_copy(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_SET(0);
			return Wg_Call(context->builtins.set, argv, 1);
		}
		
		static Wg_Obj* set_add(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_SET(0);
			argv[0]->Get<WSet>().insert(argv[1]);
			return Wg_CreateNone(context);
		}

		static Wg_Obj* set_remove(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_SET(0);
			
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
				return Wg_CreateNone(context);
			}
		}

		static Wg_Obj* set_discard(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_SET(0);

			WSet::const_iterator it{};
			auto& set = argv[0]->Get<WSet>();
			try {
				it = set.find(argv[1]);
			} catch (HashException&) {
				return nullptr;
			}

			if (it != WSet::const_iterator{})
				set.erase(it);
			return Wg_CreateNone(context);
		}

		static Wg_Obj* set_pop(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_SET(0);
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
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_SET(0);

			auto f = [](Wg_Obj* obj, void* ud) {
				auto set = (WSet*)ud;
				try {
					set->insert(obj);
					return true;
				} catch (HashException&) {
					return false;
				}
			};

			if (!Wg_Iterate(argv[1], &argv[0]->Get<WSet>(), f))
				return nullptr;
			
			return Wg_CreateNone(context);
		}

		static Wg_Obj* set_union(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT_AT_LEAST(1);
			EXPECT_ARG_TYPE_SET(0);

			Wg_Obj* res = Wg_CreateSet(context);
			WObjRef ref(res);
			
			auto f = [](Wg_Obj* obj, void* ud) {
				try {
					((WSet*)ud)->insert(obj);
					return true;
				} catch (HashException&) {
					return false;
				}
			};

			for (int i = 0; i < argc; i++)
				if (!Wg_Iterate(argv[i], &res->Get<WSet>(), f))
					return nullptr;

			return res;
		}

		static Wg_Obj* set_difference(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT_AT_LEAST(1);
			EXPECT_ARG_TYPE_SET(0);

			Wg_Obj* res = Wg_CreateSet(context);
			WObjRef ref(res);

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
					return true;
				} catch (HashException&) {
					return false;
				}
			};

			if (!Wg_Iterate(argv[0], &s, f))
				return nullptr;

			return res;
		}

		static Wg_Obj* set_intersection(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT_AT_LEAST(1);
			EXPECT_ARG_TYPE_SET(0);

			Wg_Obj* res = Wg_CreateSet(context);
			WObjRef ref(res);

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
					return true;
				} catch (HashException&) {
					return false;
				}
			};

			if (!Wg_Iterate(argv[0], &s, f))
				return nullptr;

			return res;
		}

		static Wg_Obj* set_symmetric_difference(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_SET(0);

			Wg_Obj* res = Wg_CreateSet(context);
			WObjRef ref(res);

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
					return true;
				} catch (HashException&) {
					return false;
				}
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
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_SET(0);

			Wg_Obj* inters = Wg_CallMethod(argv[0], "intersection", argv + 1, 1);
			if (inters == nullptr)
				return nullptr;

			return Wg_UnaryOp(WG_UOP_NOT, inters);
		}

		static Wg_Obj* set_issubset(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_SET(0);

			size_t size = argv[0]->Get<WSet>().size();

			Wg_Obj* inters = Wg_CallMethod(argv[0], "intersection", argv + 1, 1);
			if (inters == nullptr)
				return nullptr;
			
			if (!Wg_IsSet(inters)) {
				return Wg_CreateBool(context, false);
			}

			return Wg_CreateBool(context, inters->Get<WSet>().size() == size);
		}

		static Wg_Obj* set_issuperset(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			EXPECT_ARG_TYPE_SET(0);

			struct State {
				Wg_Obj* self;
				bool result;
			} s = { argv[0], true };

			auto f = [](Wg_Obj* obj, void* ud) {
				auto s = (State*)ud;
				Wg_Obj* contains = Wg_BinaryOp(WG_BOP_IN, obj, s->self);
				if (contains == nullptr)
					return false;
				else if (!Wg_GetBool(contains)) {
					s->result = false;
					return false;
				}
				return true;
			};

			if (!Wg_Iterate(argv[1], &s, f) && s.result)
				return nullptr;
			Wg_ClearCurrentException(context);
			return Wg_CreateBool(context, s.result);
		}

		static Wg_Obj* func_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			EXPECT_ARG_TYPE_FUNC(0);
			std::string s = "<function at " + PtrToString(argv[0]) + ">";
			return Wg_CreateString(context, s.c_str());
		}

		static Wg_Obj* BaseException_str(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			return Wg_GetAttribute(argv[0], "_message");
		}

		static Wg_Obj* DictKeysIter_next(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			void* data{};
			if (!Wg_TryGetUserdata(argv[0], "__DictKeysIter", &data)) {
				Wg_RaiseArgumentTypeError(context, 0, "__DictKeysIter");
				return nullptr;
			}
			
			auto& it = *(WDict::iterator*)data;
			it.Revalidate();
			if (it == WDict::iterator{}) {
				Wg_RaiseStopIteration(context);
				return nullptr;
			}
			
			Wg_Obj* key = it->first;
			++it;
			return key;
		}

		static Wg_Obj* DictValuesIter_next(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			void* data{};
			if (!Wg_TryGetUserdata(argv[0], "__DictValuesIter", &data)) {
				Wg_RaiseArgumentTypeError(context, 0, "__DictValuesIter");
				return nullptr;
			}

			auto& it = *(WDict::iterator*)data;
			it.Revalidate();
			if (it == WDict::iterator{}) {
				Wg_RaiseStopIteration(context);
				return nullptr;
			}

			Wg_Obj* value = it->second;
			++it;
			return value;
		}

		static Wg_Obj* DictItemsIter_next(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			void* data{};
			if (!Wg_TryGetUserdata(argv[0], "__DictItemsIter", &data)) {
				Wg_RaiseArgumentTypeError(context, 0, "__DictItemsIter");
				return nullptr;
			}

			auto& it = *(WDict::iterator*)data;
			it.Revalidate();
			if (it == WDict::iterator{}) {
				Wg_RaiseStopIteration(context);
				return nullptr;
			}

			Wg_Obj* tup[2] = { it->first, it->second };
			++it;
			return Wg_CreateTuple(context, tup, 2);
		}

		static Wg_Obj* SetIter_next(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			void* data{};
			if (!Wg_TryGetUserdata(argv[0], "__SetIter", &data)) {
				Wg_RaiseArgumentTypeError(context, 0, "__SetIter");
				return nullptr;
			}

			auto& it = *(WSet::iterator*)data;
			it.Revalidate();
			if (it == WSet::iterator{}) {
				Wg_RaiseStopIteration(context);
				return nullptr;
			}
			
			Wg_Obj* obj = *it;
			++it;
			return obj;
		}
		
		static Wg_Obj* self(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			return argv[0];
		}

	} // namespace methods

	namespace lib {

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
			return Wg_CreateNone(context);
		}

		static Wg_Obj* isinstance(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(2);
			bool ret{};
			if (Wg_IsTuple(argv[1])) {
				const auto& buf = argv[1]->Get<std::vector<Wg_Obj*>>();
				ret = Wg_IsInstance(argv[0], buf.data(), (int)buf.size()) != nullptr;
			} else {
				ret = Wg_IsInstance(argv[0], argv + 1, 1) != nullptr;
			}
			return Wg_CreateBool(context, ret);
		}

		static Wg_Obj* len(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			Wg_Obj* res = Wg_CallMethod(argv[0], "__len__", nullptr, 0);
			if (res == nullptr) {
				return nullptr;
			} else if (!Wg_IsInt(res)) {
				Wg_RaiseTypeError(context, "__len__() returned a non int type");
				return nullptr;
			} else if (Wg_GetInt(res) < 0) {
				Wg_RaiseValueError(context, "__len__() returned a negative number");
				return nullptr;
			}
			return res;
		}

		static Wg_Obj* repr(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			Wg_Obj* res = Wg_CallMethod(argv[0], "__repr__", nullptr, 0);
			if (res == nullptr) {
				return nullptr;
			} else if (!Wg_IsString(res)) {
				Wg_RaiseTypeError(context, "__repr__() returned a non string type");
				return nullptr;
			}
			return res;
		}

		static Wg_Obj* next(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			return Wg_CallMethod(argv[0], "__next__", nullptr, 0);
		}

		static Wg_Obj* iter(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			return Wg_CallMethod(argv[0], "__iter__", nullptr, 0);
		}

		static Wg_Obj* reversed(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			return Wg_CallMethod(argv[0], "__reversed__", nullptr, 0);
		}

		static Wg_Obj* abs(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			return Wg_CallMethod(argv[0], "__abs__", nullptr, 0);
		}

		static Wg_Obj* hash(Wg_Context* context, Wg_Obj** argv, int argc) {
			EXPECT_ARG_COUNT(1);
			Wg_Obj* res = Wg_CallMethod(argv[0], "__hash__", nullptr, 0);
			if (res == nullptr) {
				return nullptr;
			} else if (!Wg_IsInt(res)) {
				Wg_RaiseTypeError(context, "__hash__() returned a non int type");
				return nullptr;
			}
			return res;
		}

		static Wg_Obj* exit(Wg_Context* context, Wg_Obj** argv, int argc) {
			Wg_RaiseSystemExit(context);
			return nullptr;
		}

	} // namespace lib

	struct LibraryInitException : std::exception {};

	using WFuncSignature = Wg_Obj * (*)(Wg_Context*, Wg_Obj**, int);

	template <WFuncSignature fn>
	void RegisterMethod(Wg_Obj* _class, const char* name) {
		Wg_FuncDesc wfn{};
		wfn.isMethod = true;
		wfn.prettyName = name;
		wfn.userdata = _class->context;
		wfn.fptr = fn;

		Wg_Obj* method = Wg_CreateFunction(_class->context, &wfn);
		if (method == nullptr)
			throw LibraryInitException();

		if (Wg_IsClass(_class)) {
			Wg_AddAttributeToClass(_class, name, method);
		} else {
			Wg_SetAttribute(_class, name, method);
		}
	}

	template <WFuncSignature fn>
	Wg_Obj* RegisterFunction(Wg_Context* context, const char* name) {
		Wg_FuncDesc wfn{};
		wfn.isMethod = true;
		wfn.prettyName = name;
		wfn.userdata = context;
		wfn.fptr = fn;

		Wg_Obj* obj = Wg_CreateFunction(context, &wfn);
		if (obj == nullptr)
			throw LibraryInitException();
		Wg_SetGlobal(context, name, obj);
		return obj;
	}

	Wg_Obj* CreateClass(Wg_Context* context, const char* name) {
		Wg_Obj* _class = Wg_CreateClass(context, name, nullptr, 0);
		if (_class == nullptr)
			throw LibraryInitException();
		return _class;
	}

	void InitLibrary(Wg_Context* context) {
		try {
			auto getGlobal = [&](const char* name) {
				if (Wg_Obj* v = Wg_GetGlobal(context, name))
					return v;
				throw LibraryInitException();
			};

			auto createClass = [&](const char* name, Wg_Obj* base = nullptr, bool assign = true) {
				if (Wg_Obj* v = Wg_CreateClass(context, name, &base, base ? 1 : 0)) {
					if (assign)
						Wg_SetGlobal(context, name, v);
					return v;
				}
				throw LibraryInitException();
			};

			// Create object class
			context->builtins.object = Alloc(context);
			if (context->builtins.object == nullptr)
				throw LibraryInitException();
			context->builtins.object->type = "__class";
			context->builtins.object->data = new Wg_Obj::Class{ std::string("object") };
			context->builtins.object->finalizer.fptr = [](Wg_Obj* obj, void*) { delete (Wg_Obj::Class*)obj->data; };
			context->builtins.object->Get<Wg_Obj::Class>().instanceAttributes.Set("__class__", context->builtins.object);
			context->builtins.object->attributes.AddParent(context->builtins.object->Get<Wg_Obj::Class>().instanceAttributes);
			context->builtins.object->Get<Wg_Obj::Class>().userdata = context;
			context->builtins.object->Get<Wg_Obj::Class>().ctor = ctors::object;
			Wg_SetGlobal(context, "object", context->builtins.object);

			// Create function class
			context->builtins.func = Alloc(context);
			if (context->builtins.func == nullptr)
				throw LibraryInitException();
			context->builtins.func->type = "__class";
			context->builtins.func->data = new Wg_Obj::Class{ std::string("function") };
			context->builtins.func->finalizer.fptr = [](Wg_Obj* obj, void*) { delete (Wg_Obj::Class*)obj->data; };
			context->builtins.func->Get<Wg_Obj::Class>().instanceAttributes.Set("__class__", context->builtins.func);
			context->builtins.func->attributes.AddParent(context->builtins.object->Get<Wg_Obj::Class>().instanceAttributes);
			context->builtins.func->Get<Wg_Obj::Class>().userdata = context;
			context->builtins.func->Get<Wg_Obj::Class>().ctor = ctors::func;
			RegisterMethod<methods::func_str>(context->builtins.func, "__str__");

			// Create tuple class
			context->builtins.tuple = Alloc(context);
			context->builtins.tuple->type = "__class";
			context->builtins.tuple->data = new Wg_Obj::Class{ std::string("tuple") };
			context->builtins.tuple->finalizer.fptr = [](Wg_Obj* obj, void*) { delete (Wg_Obj::Class*)obj->data; };
			context->builtins.tuple->Get<Wg_Obj::Class>().instanceAttributes.Set("__class__", context->builtins.tuple);
			context->builtins.tuple->attributes.AddParent(context->builtins.object->Get<Wg_Obj::Class>().instanceAttributes);
			context->builtins.tuple->Get<Wg_Obj::Class>().userdata = context;
			context->builtins.tuple->Get<Wg_Obj::Class>().ctor = ctors::tuple;
			Wg_SetGlobal(context, "tuple", context->builtins.tuple);
			RegisterMethod<methods::collection_str<Collection::Tuple>>(context->builtins.tuple, "__str__");
			RegisterMethod<methods::collection_getitem<Collection::Tuple>>(context->builtins.tuple, "__getitem__");
			RegisterMethod<methods::collection_len<Collection::Tuple>>(context->builtins.tuple, "__len__");
			RegisterMethod<methods::collection_contains<Collection::Tuple>>(context->builtins.tuple, "__contains__");
			RegisterMethod<methods::collection_eq<Collection::Tuple>>(context->builtins.tuple, "__eq__");
			RegisterMethod<methods::collection_lt<Collection::Tuple>>(context->builtins.tuple, "__lt__");
			RegisterMethod<methods::collection_nonzero<Collection::Tuple>>(context->builtins.tuple, "__nonzero__");
			RegisterMethod<methods::object_iter>(context->builtins.tuple, "__iter__");
			RegisterMethod<methods::collection_count<Collection::Tuple>>(context->builtins.tuple, "count");
			RegisterMethod<methods::collection_index<Collection::Tuple>>(context->builtins.tuple, "index");

			// Create NoneType class
			context->builtins.none = Alloc(context);
			context->builtins.none->type = "__class";
			context->builtins.none->data = new Wg_Obj::Class{ std::string("NoneType") };
			context->builtins.none->finalizer.fptr = [](Wg_Obj* obj, void*) { delete (Wg_Obj::Class*)obj->data; };
			context->builtins.none->Get<Wg_Obj::Class>().instanceAttributes.Set("__class__", context->builtins.none);
			context->builtins.none->attributes.AddParent(context->builtins.object->Get<Wg_Obj::Class>().instanceAttributes);
			context->builtins.none->Get<Wg_Obj::Class>().userdata = context;
			context->builtins.none->Get<Wg_Obj::Class>().ctor = ctors::none;

			// Create None singleton
			context->builtins.none = Alloc(context);
			context->builtins.none->type = "__null";
			Wg_SetAttribute(context->builtins.none, "__class__", context->builtins.none);
			context->builtins.none->attributes.AddParent(context->builtins.object->Get<Wg_Obj::Class>().instanceAttributes);
			RegisterMethod<methods::null_nonzero>(context->builtins.none, "__nonzero__");
			RegisterMethod<methods::null_str>(context->builtins.none, "__str__");

			// Add __bases__ tuple to the classes created before
			Wg_Obj* emptyTuple = Wg_CreateTuple(context, nullptr, 0);
			if (emptyTuple == nullptr)
				throw LibraryInitException();
			Wg_SetAttribute(context->builtins.object, "__bases__", emptyTuple);
			Wg_SetAttribute(context->builtins.none, "__bases__", emptyTuple);
			Wg_SetAttribute(context->builtins.func, "__bases__", emptyTuple);
			Wg_SetAttribute(context->builtins.tuple, "__bases__", emptyTuple);

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
			context->builtins._bool->Get<Wg_Obj::Class>().ctor = ctors::_bool;
			RegisterMethod<methods::bool_nonzero>(context->builtins._bool, "__nonzero__");
			RegisterMethod<methods::bool_int>(context->builtins._bool, "__int__");
			RegisterMethod<methods::bool_float>(context->builtins._bool, "__float__");
			RegisterMethod<methods::bool_str>(context->builtins._bool, "__str__");
			RegisterMethod<methods::bool_eq>(context->builtins._bool, "__eq__");
			RegisterMethod<methods::bool_hash>(context->builtins._bool, "__hash__");
			RegisterMethod<methods::bool_abs>(context->builtins._bool, "__abs__");

			Wg_Obj* _false = Alloc(context);
			if (_false == nullptr)
				throw LibraryInitException();
			_false->attributes = context->builtins._bool->Get<Wg_Obj::Class>().instanceAttributes.Copy();
			_false->type = "__bool";
			_false->data = new bool(false);
			_false->finalizer.fptr = [](Wg_Obj* obj, void*) { delete (bool*)obj->data; };
			context->builtins._false = _false;
			Wg_Obj* _true = Alloc(context);
			if (_true == nullptr)
				throw LibraryInitException();
			_true->attributes = context->builtins._bool->Get<Wg_Obj::Class>().instanceAttributes.Copy();
			_true->type = "__bool";
			_true->data = new bool(true);
			_true->finalizer.fptr = [](Wg_Obj* obj, void*) { delete (bool*)obj->data; };
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
			RegisterMethod<ctors::list>(context->builtins.list, "__init__");
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
			RegisterMethod<methods::map_setdefault>(context->builtins.dict, "setdefault");
			RegisterMethod<methods::map_update>(context->builtins.dict, "update");

			context->builtins.set = createClass("set");
			RegisterMethod<ctors::set>(context->builtins.set, "__init__");
			RegisterMethod<methods::set_nonzero>(context->builtins.set, "__nonzero__");
			RegisterMethod<methods::set_str>(context->builtins.set, "__str__");
			RegisterMethod<methods::set_contains>(context->builtins.set, "__contains__");
			RegisterMethod<methods::set_iter>(context->builtins.set, "__iter__");
			RegisterMethod<methods::set_len>(context->builtins.set, "__len__");
			RegisterMethod<methods::set_add>(context->builtins.set, "add");
			RegisterMethod<methods::set_clear>(context->builtins.set, "clear");
			RegisterMethod<methods::set_copy>(context->builtins.set, "copy");
			RegisterMethod<methods::set_difference>(context->builtins.set, "difference");
			RegisterMethod<methods::set_discard>(context->builtins.set, "discard");
			RegisterMethod<methods::set_intersection>(context->builtins.set, "intersection");
			RegisterMethod<methods::set_isdisjoint>(context->builtins.set, "isdisjoint");
			RegisterMethod<methods::set_issubset>(context->builtins.set, "issubset");
			RegisterMethod<methods::set_issuperset>(context->builtins.set, "issuperset");
			RegisterMethod<methods::set_pop>(context->builtins.set, "pop");
			RegisterMethod<methods::set_remove>(context->builtins.set, "remove");
			RegisterMethod<methods::set_symmetric_difference>(context->builtins.set, "symmetric_difference");
			RegisterMethod<methods::set_union>(context->builtins.set, "union");
			RegisterMethod<methods::set_update>(context->builtins.set, "update");

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

			context->builtins.setIter = createClass("__SetIter", nullptr, false);
			RegisterMethod<ctors::SetIter>(context->builtins.setIter, "__init__");
			RegisterMethod<methods::SetIter_next>(context->builtins.setIter, "__next__");
			RegisterMethod<methods::self>(context->builtins.setIter, "__iter__");

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
			RegisterFunction<lib::exit>(context, "exit");

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
			context->builtins.systemExit = createClass("SystemExit", context->builtins.baseException);

			// Initialize the rest with a script
			Wg_Obj* lib = Wg_Compile(context, LIBRARY_CODE, "__builtins__");
			if (lib == nullptr)
				throw LibraryInitException();
			if (Wg_Call(lib, nullptr, 0) == nullptr)
				throw LibraryInitException();

			context->builtins.slice = getGlobal("slice");
			context->builtins.defaultIter = getGlobal("__DefaultIter");
			context->builtins.defaultReverseIter = getGlobal("__DefaultReverseIter");

		} catch (LibraryInitException&) {
			std::abort(); // Internal error
		}
	}
} // namespace wings
