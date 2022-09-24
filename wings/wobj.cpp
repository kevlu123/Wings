#include "impl.h"
#include "gc.h"
#include "executor.h"
#include <algorithm>
#include <queue>

extern "C" {

	Wg_Obj* Wg_CreateNone(Wg_Context* context) {
		WASSERT(context);
		return context->builtins.none;
	}

	Wg_Obj* Wg_CreateBool(Wg_Context* context, bool value) {
		WASSERT(context);
		if (value && context->builtins._true) {
			return context->builtins._true;
		} else if (!value && context->builtins._false) {
			return context->builtins._false;
		} else {
			return value ? context->builtins._true : context->builtins._false;
		}
	}

	Wg_Obj* Wg_CreateInt(Wg_Context* context, Wg_int value) {
		WASSERT(context);
		if (Wg_Obj* v = Wg_Call(context->builtins._int, nullptr, 0)) {
			v->Get<Wg_int>() = value;
			return v;
		} else {
			return nullptr;
		}
	}

	Wg_Obj* Wg_CreateFloat(Wg_Context* context, Wg_float value) {
		WASSERT(context);
		if (Wg_Obj* v = Wg_Call(context->builtins._float, nullptr, 0)) {
			v->Get<Wg_float>() = value;
			return v;
		} else {
			return nullptr;
		}
	}

	Wg_Obj* Wg_CreateString(Wg_Context* context, const char* value) {
		WASSERT(context);
		if (Wg_Obj* v = Wg_Call(context->builtins.str, nullptr, 0)) {
			v->Get<std::string>() = value ? value : "";
			return v;
		} else {
			return nullptr;
		}
	}

	Wg_Obj* Wg_CreateTuple(Wg_Context* context, Wg_Obj** argv, int argc) {
		std::vector<wings::WObjRef> refs;
		WASSERT(context && argc >= 0);
		if (argc > 0) {
			WASSERT(argv);
			for (int i = 0; i < argc; i++) {
				refs.emplace_back(argv[i]);
				WASSERT(argv[i]);
			}
		}

		if (Wg_Obj* v = Wg_Call(context->builtins.tuple, nullptr, 0)) {
			v->Get<std::vector<Wg_Obj*>>() = std::vector<Wg_Obj*>(argv, argv + argc);
			return v;
		} else {
			return nullptr;
		}
	}

	Wg_Obj* Wg_CreateList(Wg_Context* context, Wg_Obj** argv, int argc) {
		std::vector<wings::WObjRef> refs;
		WASSERT(context && argc >= 0);
		if (argc > 0) {
			WASSERT(argv);
			for (int i = 0; i < argc; i++) {
				refs.emplace_back(argv[i]);
				WASSERT(argv[i]);
			}
		}

		if (Wg_Obj* v = Wg_Call(context->builtins.list, nullptr, 0)) {
			v->Get<std::vector<Wg_Obj*>>() = std::vector<Wg_Obj*>(argv, argv + argc);
			return v;
		} else {
			return nullptr;
		}
	}

	Wg_Obj* Wg_CreateDictionary(Wg_Context* context, Wg_Obj** keys, Wg_Obj** values, int argc) {
		std::vector<wings::WObjRef> refs;
		WASSERT(context && argc >= 0);
		if (argc > 0) {
			WASSERT(keys && values);
			for (int i = 0; i < argc; i++) {
				refs.emplace_back(keys[i]);
				refs.emplace_back(values[i]);
				WASSERT(keys[i] && values[i]);
			}
		}

		// Pass a dummy kwargs to prevent stack overflow from recursion
		Wg_Obj* dummyKwargs = wings::Alloc(context);
		if (dummyKwargs == nullptr)
			return nullptr;
		dummyKwargs->type = "__map";
		wings::WDict wd{};
		dummyKwargs->data = &wd;

		if (Wg_Obj* v = Wg_Call(context->builtins.dict, nullptr, 0, dummyKwargs)) {
			for (int i = 0; i < argc; i++) {
				refs.emplace_back(v);
				try {
					v->Get<wings::WDict>()[keys[i]] = values[i];
				} catch (wings::HashException&) {
					return nullptr;
				}
			}
			return v;
		} else {
			return nullptr;
		}
	}

	Wg_Obj* Wg_CreateSet(Wg_Context* context, Wg_Obj** argv, int argc) {
		std::vector<wings::WObjRef> refs;
		WASSERT(context && argc >= 0);
		if (argc > 0) {
			WASSERT(argv);
			for (int i = 0; i < argc; i++) {
				refs.emplace_back(argv[i]);
				WASSERT(argv[i]);
			}
		}

		if (Wg_Obj* v = Wg_Call(context->builtins.set, nullptr, 0, nullptr)) {
			for (int i = 0; i < argc; i++) {
				try {
					v->Get<wings::WSet>().insert(argv[i]);
				} catch (wings::HashException&) {
					return nullptr;
				}
			}
			return v;
		} else {
			return nullptr;
		}
	}

	Wg_Obj* Wg_CreateFunction(Wg_Context* context, const Wg_FuncDesc* value) {
		WASSERT(context && value);
		if (Wg_Obj* v = Wg_Call(context->builtins.func, nullptr, 0)) {
			v->Get<Wg_Obj::Func>() = {
				nullptr,
				value->fptr,
				value->userdata,
				value->isMethod,
				std::string(value->tag ? value->tag : wings::DEFAULT_TAG_NAME),
				std::string(value->prettyName ? value->prettyName : wings::DEFAULT_FUNC_NAME)
			};
			return v;
		} else {
			return nullptr;
		}
	}

	Wg_Obj* Wg_CreateClass(Wg_Context* context, const char* name, Wg_Obj** bases, int baseCount) {
		std::vector<wings::WObjRef> refs;
		WASSERT(context && name && baseCount >= 0);
		if (baseCount > 0) {
			WASSERT(bases);
			for (int i = 0; i < baseCount; i++) {
				WASSERT(bases[i] && Wg_IsClass(bases[i]));
				refs.emplace_back(bases[i]);
			}
		}

		// Allocate class
		Wg_Obj* _class = wings::Alloc(context);
		if (_class == nullptr) {
			return nullptr;
		}
		refs.emplace_back(_class);
		_class->type = "__class";
		_class->data = new Wg_Obj::Class{ std::string(name) };
		_class->finalizer.fptr = [](Wg_Obj* obj, void*) { delete (Wg_Obj::Class*)obj->data; };
		_class->Get<Wg_Obj::Class>().instanceAttributes.Set("__class__", _class);
		_class->attributes.AddParent(context->builtins.object->Get<Wg_Obj::Class>().instanceAttributes);

		// Set bases
		int actualBaseCount = baseCount ? baseCount : 1;
		Wg_Obj** actualBases = baseCount ? bases : &context->builtins.object;
		for (int i = 0; i < actualBaseCount; i++) {
			_class->Get<Wg_Obj::Class>().instanceAttributes.AddParent(actualBases[i]->Get<Wg_Obj::Class>().instanceAttributes);
			_class->Get<Wg_Obj::Class>().bases.push_back(actualBases[i]);
		}
		if (Wg_Obj* basesTuple = Wg_CreateTuple(context, actualBases, actualBaseCount)) {
			_class->attributes.Set("__bases__", basesTuple);
		} else {
			return nullptr;
		}

		// Set __str__()
		Wg_FuncDesc tostr{};
		tostr.isMethod = true;
		tostr.prettyName = "__str__";
		tostr.userdata = context;
		tostr.fptr = [](Wg_Obj** argv, int argc, Wg_Obj* kwargs, void* ud) -> Wg_Obj* {
			if (argc != 1) {
				Wg_RaiseArgumentCountError((Wg_Context*)ud, argc, 1);
				return nullptr;
			}
			std::string s = "<class '" + argv[0]->Get<Wg_Obj::Class>().name + "'>";
			return Wg_CreateString(argv[0]->context, s.c_str());
		};
		if (Wg_Obj* tostrFn = Wg_CreateFunction(context, &tostr)) {
			Wg_SetAttribute(_class, "__str__", tostrFn);
		} else {
			return nullptr;
		}

		// Set construction function. This function forwards to __init__().
		_class->Get<Wg_Obj::Class>().userdata = _class;
		_class->Get<Wg_Obj::Class>().ctor = [](Wg_Obj** argv, int argc, Wg_Obj* kwargs, void* userdata) -> Wg_Obj* {
			Wg_Obj* _classObj = (Wg_Obj*)userdata;
			Wg_Context* context = _classObj->context;

			Wg_Obj* instance = wings::Alloc(context);
			if (instance == nullptr)
				return nullptr;
			wings::WObjRef ref(instance);

			instance->attributes = _classObj->Get<Wg_Obj::Class>().instanceAttributes.Copy();
			instance->type = _classObj->Get<Wg_Obj::Class>().name;

			if (Wg_Obj* init = Wg_HasAttribute(instance, "__init__")) {
				if (Wg_IsFunction(init)) {
					Wg_Obj* ret = Wg_Call(init, argv, argc, kwargs);
					if (ret == nullptr) {
						return nullptr;
					} else if (!Wg_IsNone(ret)) {
						Wg_RaiseTypeError(context, "__init__() returned a non NoneType type");
						return nullptr;
					}
				}
			}

			return instance;
		};

		// Set init method
		std::string initName = std::string(name) + ".__init__";
		Wg_FuncDesc init{};
		init.prettyName = initName.c_str();
		init.isMethod = true;
		init.userdata = _class;
		init.fptr = [](Wg_Obj** argv, int argc, Wg_Obj* kwargs, void* userdata) -> Wg_Obj* {
			Wg_Obj* _class = (Wg_Obj*)userdata;
			if (argc < 1) {
				Wg_RaiseArgumentCountError(_class->context, argc, -1);
				return nullptr;
			}

			const auto& bases = _class->Get<Wg_Obj::Class>().bases;
			if (bases.empty())
				return nullptr;

			if (Wg_Obj* baseInit = Wg_GetAttributeFromBase(argv[0], "__init__", bases[0])) {
				Wg_Obj* ret = Wg_Call(baseInit, argv + 1, argc - 1, kwargs);
				if (ret == nullptr) {
					return nullptr;
				} else if (!Wg_IsNone(ret)) {
					Wg_RaiseTypeError(argv[0]->context, "__init__() returned a non NoneType type");
					return nullptr;
				}
			}

			return Wg_CreateNone(argv[0]->context);
		};
		Wg_Obj* initFn = Wg_CreateFunction(context, &init);
		if (initFn == nullptr)
			return nullptr;
		Wg_LinkReference(initFn, _class);
		Wg_AddAttributeToClass(_class, "__init__", initFn);

		return _class;
	}
	
	void Wg_AddAttributeToClass(Wg_Obj* class_, const char* attribute, Wg_Obj* value) {
		WASSERT_VOID(class_ && attribute && value && Wg_IsClass(class_));
		class_->Get<Wg_Obj::Class>().instanceAttributes.Set(attribute, value);
	}

	bool Wg_IsNone(const Wg_Obj* obj) {
		WASSERT(obj);
		return obj == obj->context->builtins.none;
	}

	bool Wg_IsBool(const Wg_Obj* obj) {
		WASSERT(obj);
		return obj == obj->context->builtins._true
			|| obj == obj->context->builtins._false;
	}

	bool Wg_IsInt(const Wg_Obj* obj) {
		WASSERT(obj);
		return obj->type == "__int";
	}

	bool Wg_IsIntOrFloat(const Wg_Obj* obj) {
		WASSERT(obj);
		return obj->type == "__int" || obj->type == "__float";
	}

	bool Wg_IsString(const Wg_Obj* obj) {
		WASSERT(obj);
		return obj->type == "__str";
	}

	bool Wg_IsTuple(const Wg_Obj* obj) {
		WASSERT(obj);
		return obj->type == "__tuple";
	}

	bool Wg_IsList(const Wg_Obj* obj) {
		WASSERT(obj);
		return obj->type == "__list";
	}

	bool Wg_IsDictionary(const Wg_Obj* obj) {
		WASSERT(obj);
		return obj->type == "__map";
	}

	bool Wg_IsSet(const Wg_Obj* obj) {
		WASSERT(obj);
		return obj->type == "__set";
	}

	bool Wg_IsClass(const Wg_Obj* obj) {
		WASSERT(obj);
		return obj->type == "__class";
	}

	bool Wg_IsFunction(const Wg_Obj* obj) {
		WASSERT(obj);
		return obj->type == "__func";
	}

	bool Wg_GetBool(const Wg_Obj* obj) {
		WASSERT(obj && Wg_IsBool(obj));
		return obj->Get<bool>();
	}

	Wg_int Wg_GetInt(const Wg_Obj* obj) {
		WASSERT(obj && Wg_IsInt(obj));
		return obj->Get<Wg_int>();
	}

	Wg_float Wg_GetFloat(const Wg_Obj* obj) {
		WASSERT(obj && Wg_IsIntOrFloat(obj));
		if (Wg_IsInt(obj)) return (Wg_float)obj->Get<Wg_int>();
		else return obj->Get<Wg_float>();
	}

	const char* Wg_GetString(const Wg_Obj* obj) {
		WASSERT(obj && Wg_IsString(obj));
		return obj->Get<std::string>().c_str();
	}

	void Wg_SetUserdata(Wg_Obj* obj, void* userdata) {
		WASSERT_VOID(obj);
		obj->data = userdata;
	}

	bool Wg_TryGetUserdata(const Wg_Obj* obj, const char* type, void** out) {
		WASSERT(obj && type);
		if (obj->type == std::string(type)) {
			*out = obj->data;
			return true;
		} else {
			return false;
		}
	}

	void Wg_GetFinalizer(const Wg_Obj* obj, Wg_FinalizerDesc* out) {
		WASSERT_VOID(obj && out);
		*out = obj->finalizer;
	}

	void Wg_SetFinalizer(Wg_Obj* obj, const Wg_FinalizerDesc* finalizer) {
		WASSERT_VOID(obj && finalizer);
		obj->finalizer = *finalizer;
	}

	Wg_Obj* Wg_HasAttribute(Wg_Obj* obj, const char* member) {
		WASSERT(obj && member);
		Wg_Obj* mem = obj->attributes.Get(member);
		if (mem && Wg_IsFunction(mem) && mem->Get<Wg_Obj::Func>().isMethod) {
			mem->Get<Wg_Obj::Func>().self = obj;
		}
		return mem;
	}

	Wg_Obj* Wg_GetAttribute(Wg_Obj* obj, const char* member) {
		WASSERT(obj && member);
		Wg_Obj* mem = obj->attributes.Get(member);
		if (mem == nullptr) {
			Wg_RaiseAttributeError(obj, member);
		} else if (Wg_IsFunction(mem) && mem->Get<Wg_Obj::Func>().isMethod) {
			mem->Get<Wg_Obj::Func>().self = obj;
		}
		return mem;
	}

	void Wg_SetAttribute(Wg_Obj* obj, const char* member, Wg_Obj* value) {
		WASSERT_VOID(obj && member && value);
		obj->attributes.Set(member, value);
	}

	Wg_Obj* Wg_GetAttributeFromBase(Wg_Obj* obj, const char* member, Wg_Obj* baseClass) {
		WASSERT(obj && member);

		Wg_Obj* mem{};
		if (baseClass == nullptr) {
			mem = obj->attributes.GetFromBase(member);
		} else {
			mem = baseClass->Get<Wg_Obj::Class>().instanceAttributes.Get(member);
		}

		if (mem && Wg_IsFunction(mem) && mem->Get<Wg_Obj::Func>().isMethod) {
			mem->Get<Wg_Obj::Func>().self = obj;
		}
		return mem;
	}

	Wg_Obj* Wg_IsInstance(const Wg_Obj* instance, Wg_Obj*const* types, int typesLen) {
		WASSERT(instance && typesLen >= 0 && (types || typesLen == 0));
		for (int i = 0; i < typesLen; i++)
			WASSERT(types[i] && Wg_IsClass(types[i]));

		// Cannot use Wg_HasAttribute here because instance is a const pointer
		Wg_Obj* _class = instance->attributes.Get("__class__");
		if (_class == nullptr)
			return nullptr;
		wings::WObjRef ref(_class);

		std::queue<wings::WObjRef> toCheck;
		toCheck.emplace(_class);

		while (!toCheck.empty()) {
			auto end = types + typesLen;
			auto it = std::find(types, end, toCheck.front().Get());
			if (it != end)
				return *it;

			Wg_Obj* bases = Wg_HasAttribute(toCheck.front().Get(), "__bases__");
			if (bases && Wg_IsTuple(bases))
				for (Wg_Obj* base : bases->Get<std::vector<Wg_Obj*>>())
					toCheck.emplace(base);

			toCheck.pop();
		}
		return nullptr;
	}

	bool Wg_Iterate(Wg_Obj* obj, void* userdata, Wg_IterationCallback callback) {
		WASSERT(obj && callback);
		Wg_Context* context = obj->context;

		Wg_Obj* iter = Wg_CallMethod(obj, "__iter__", nullptr, 0);
		if (iter == nullptr)
			return false;
		wings::WObjRef iterRef(iter);

		while (true) {
			Wg_Obj* yielded = Wg_CallMethod(iter, "__next__", nullptr, 0);
			wings::WObjRef yieldedRef(yielded);
			if (yielded)
				callback(yielded, userdata);

			Wg_Obj* exc = Wg_GetCurrentException(context);
			if (exc) {
				if (Wg_IsInstance(exc, &context->builtins.stopIteration, 1)) {
					Wg_ClearCurrentException(context);
					return true;
				} else {
					return false;
				}
			}
		}
	}

	bool Wg_Unpack(Wg_Obj* obj, Wg_Obj** out, int count) {
		WASSERT(obj && (count == 0 || out));
		
		Wg_Context* context = obj->context;
		struct State {
			Wg_Context* context;
			Wg_Obj** array;
			int count;
			int index;
		} s = { context, out, count, 0 };
		
		bool success = Wg_Iterate(obj, &s, [](Wg_Obj* yielded, void* userdata) {
			State* s = (State*)userdata;
			if (s->index >= s->count) {
				Wg_RaiseValueError(s->context, "Too many values to unpack");
				return false;
			}
			Wg_ProtectObject(yielded);
			s->array[s->index] = yielded;
			s->index++;
			return true;
			});

		for (int i = s.index; i; i--)
			Wg_UnprotectObject(out[i - 1]);

		if (!success) {
			return false;
		} else if (s.index < count) {
			Wg_RaiseValueError(context, "Not enough values to unpack");
			return false;
		} else {
			return true;
		}
	}

	Wg_Obj* WConvertToBool(Wg_Obj* arg) {
		WASSERT(arg);
		return Wg_Call(arg->context->builtins._bool, &arg, 1);
	}

	Wg_Obj* WConvertToInt(Wg_Obj* arg) {
		return Wg_Call(arg->context->builtins._int, &arg, 1);
	}

	Wg_Obj* WConvertToFloat(Wg_Obj* arg) {
		return Wg_Call(arg->context->builtins._float, &arg, 1);
	}

	Wg_Obj* WConvertToString(Wg_Obj* arg) {
		return Wg_Call(arg->context->builtins.str, &arg, 1);
	}

	Wg_Obj* WRepr(Wg_Obj* arg) {
		WASSERT(arg);
		return Wg_Call(arg->context->builtins.repr, &arg, 1);
	}

	Wg_Obj* Wg_Call(Wg_Obj* callable, Wg_Obj** argv, int argc, Wg_Obj* kwargsDict) {
		WASSERT(callable && argc >= 0 && (argc == 0 || argv));
		if (Wg_IsFunction(callable) || Wg_IsClass(callable)) {
			if (argc)
				WASSERT(argv);
			for (int i = 0; i < argc; i++)
				WASSERT(argv[i]);

			if (kwargsDict) {
				if (!Wg_IsDictionary(kwargsDict)) {
					Wg_RaiseTypeError(kwargsDict->context, "Keyword arguments must be a dictionary");
					return nullptr;
				}
				for (const auto& [key, value] : kwargsDict->Get<wings::WDict>()) {
					if (!Wg_IsString(key)) {
						Wg_RaiseTypeError(kwargsDict->context, "Keyword arguments dictionary must only contain string keys");
						return nullptr;
					}
				}
			}

			std::vector<wings::WObjRef> refs;
			refs.emplace_back(callable);
			for (int i = 0; i < argc; i++)
				refs.emplace_back(argv[i]);

			Wg_Obj* (*fptr)(Wg_Obj**, int, Wg_Obj*, void*);
			void* userdata = nullptr;
			Wg_Obj* self = nullptr;
			std::string prettyName;
			if (Wg_IsFunction(callable)) {
				const auto& func = callable->Get<Wg_Obj::Func>();
				if (func.self)
					self = func.self;
				fptr = func.fptr;
				userdata = func.userdata;
				prettyName = func.prettyName;

				callable->context->currentTrace.push_back(wings::TraceFrame{
					{},
					"",
					func.tag,
					prettyName
					});
			} else {
				fptr = callable->Get<Wg_Obj::Class>().ctor;
				userdata = callable->Get<Wg_Obj::Class>().userdata;
				prettyName = callable->Get<Wg_Obj::Class>().name;
			}

			std::vector<Wg_Obj*> argsWithSelf;
			if (self) {
				argsWithSelf.push_back(self);
				refs.emplace_back(self);
			}
			argsWithSelf.insert(argsWithSelf.end(), argv, argv + argc);

			// If the dictionary (map) class doesn't exist yet then skip this
			if (kwargsDict == nullptr && callable != callable->context->builtins.dict && callable->context->builtins.dict) {
				kwargsDict = Wg_CreateDictionary(callable->context);
				if (kwargsDict == nullptr)
					return nullptr;
			}
			refs.emplace_back(kwargsDict);

			Wg_Obj* ret = fptr(argsWithSelf.data(), (int)argsWithSelf.size(), kwargsDict, userdata);

			if (Wg_IsFunction(callable)) {
				callable->context->currentTrace.pop_back();
			}

			return ret;
		} else {
			return Wg_CallMethod(callable, "__call__", argv, argc);
		}
	}

	Wg_Obj* Wg_CallMethod(Wg_Obj* obj, const char* member, Wg_Obj** argv, int argc, Wg_Obj* kwargsDict) {
		WASSERT(obj && member);
		if (argc)
			WASSERT(argv);
		for (int i = 0; i < argc; i++)
			WASSERT(argv[i]);

		if (Wg_Obj* method = Wg_GetAttribute(obj, member)) {
			return Wg_Call(method, argv, argc, kwargsDict);
		} else {
			return nullptr;
		}
	}

	Wg_Obj* Wg_CallMethodFromBase(Wg_Obj* obj, const char* member, Wg_Obj** argv, int argc, Wg_Obj* kwargsDict, Wg_Obj* baseClass) {
		WASSERT(obj && member);
		if (argc)
			WASSERT(argv);
		for (int i = 0; i < argc; i++)
			WASSERT(argv[i]);

		if (Wg_Obj* method = Wg_GetAttributeFromBase(obj, member, baseClass)) {
			return Wg_Call(method, argv, argc, kwargsDict);
		} else {
			Wg_RaiseAttributeError(obj, member);
			return nullptr;
		}
	}

	bool Wg_ParseKwargs(Wg_Obj* kwargs, const char* const* keys, int count, Wg_Obj** out) {
		WASSERT(kwargs && keys && out && count > 0 && Wg_IsDictionary(kwargs));
		
		wings::WObjRef ref(kwargs);
		auto& buf = kwargs->Get<wings::WDict>();
		for (int i = 0; i < count; i++) {
			Wg_Obj* key = Wg_CreateString(kwargs->context, keys[i]);
			if (key == nullptr)
				return false;

			wings::WDict::iterator it;
			try {
				it = buf.find(key);
			} catch (wings::HashException&) {
				return false;
			}

			if (it != buf.end()) {
				out[i] = it->second;
			} else {
				out[i] = nullptr;
			}
		}
		return true;
	}

	Wg_Obj* Wg_GetIndex(Wg_Obj* obj, Wg_Obj* index) {
		WASSERT(obj && index);
		return Wg_CallMethod(obj, "__getitem__", &index, 1);
	}

	Wg_Obj* Wg_SetIndex(Wg_Obj* obj, Wg_Obj* index, Wg_Obj* value) {
		WASSERT(obj && index && value);
		Wg_Obj* argv[2] = { index, value };
		return Wg_CallMethod(obj, "__setitem__", argv, 2);
	}

	Wg_Obj* Wg_UnaryOp(Wg_UnOp op, Wg_Obj* arg) {
		WASSERT(arg);
		switch (op) {
		case WG_UOP_POS:
			return Wg_CallMethod(arg, "__pos__", nullptr, 0);
		case WG_UOP_NEG:
			return Wg_CallMethod(arg, "__neg__", nullptr, 0);
		case WG_UOP_BITNOT:
			return Wg_CallMethod(arg, "__invert__", nullptr, 0);
		case WG_UOP_HASH:
			return Wg_Call(arg->context->builtins.hash, &arg, 1);
		case WG_UOP_LEN:
			return Wg_Call(arg->context->builtins.len, &arg, 1);
		case WG_UOP_BOOL:
			return Wg_Call(arg->context->builtins._bool, &arg, 1);
		case WG_UOP_INT:
			return Wg_Call(arg->context->builtins._int, &arg, 1);
		case WG_UOP_FLOAT:
			return Wg_Call(arg->context->builtins._float, &arg, 1);
		case WG_UOP_STR:
			return Wg_Call(arg->context->builtins.str, &arg, 1);
		case WG_UOP_REPR:
			return Wg_Call(arg->context->builtins.repr, &arg, 1);
		default:
			WUNREACHABLE();
		}
	}

	static const std::unordered_map<Wg_BinOp, const char*> OP_METHOD_NAMES = {
		{ WG_BOP_ADD, "__add__" },
		{ WG_BOP_SUB, "__sub__" },
		{ WG_BOP_MUL, "__mul__" },
		{ WG_BOP_DIV, "__truediv__" },
		{ WG_BOP_FLOORDIV, "__floordiv__" },
		{ WG_BOP_MOD, "__mod__" },
		{ WG_BOP_POW, "__pow__" },
		{ WG_BOP_BITAND, "__and__" },
		{ WG_BOP_BITOR, "__or__" },
		{ WG_BOP_BITXOR, "__not__" },
		{ WG_BOP_SHL, "__lshift__" },
		{ WG_BOP_SHR, "__rshift__" },
		{ WG_BOP_IN, "__contains__" },
		{ WG_BOP_EQ, "__eq__" },
		{ WG_BOP_NE, "__ne__" },
		{ WG_BOP_LT, "__lt__" },
		{ WG_BOP_LE, "__le__" },
		{ WG_BOP_GT, "__gt__" },
		{ WG_BOP_GE, "__ge__" },
	};
	
	Wg_Obj* Wg_BinaryOp(Wg_BinOp op, Wg_Obj* lhs, Wg_Obj* rhs) {
		WASSERT(lhs && rhs);

		if (op == WG_BOP_IN)
			std::swap(lhs, rhs);
		
		auto method = OP_METHOD_NAMES.find(op);
		
		switch (op) {
		case WG_BOP_ADD:
		case WG_BOP_SUB:
		case WG_BOP_MUL:
		case WG_BOP_DIV:
		case WG_BOP_FLOORDIV:
		case WG_BOP_MOD:
		case WG_BOP_POW:
		case WG_BOP_BITAND:
		case WG_BOP_BITOR:
		case WG_BOP_BITXOR:
		case WG_BOP_SHL:
		case WG_BOP_SHR:
			return Wg_CallMethod(lhs, method->second, &rhs, 1);
		case WG_BOP_EQ:
		case WG_BOP_NE:
		case WG_BOP_LT:
		case WG_BOP_LE:
		case WG_BOP_GT:
		case WG_BOP_GE:
		case WG_BOP_IN: {
			Wg_Obj* boolResult = Wg_CallMethod(lhs, method->second, &rhs, 1);
			if (!Wg_IsBool(boolResult)) {
				std::string message = method->second;
				message += "() returned a non bool type";
				Wg_RaiseTypeError(boolResult->context, message.c_str());
				return nullptr;
			}
			return boolResult;
		}
		case WG_BOP_NOTIN:
			if (Wg_Obj* in = Wg_BinaryOp(WG_BOP_IN, lhs, rhs)) {
				return Wg_UnaryOp(WG_UOP_NOT, in);
			} else {
				return nullptr;
			}
		case WG_BOP_AND: {
			Wg_Obj* lhsb = Wg_UnaryOp(WG_UOP_BOOL, lhs);
			if (lhsb == nullptr)
				return nullptr;
			if (!Wg_GetBool(lhsb))
				return lhsb;
			return Wg_UnaryOp(WG_UOP_BOOL, rhs);
		}
		case WG_BOP_OR: {
			Wg_Obj* lhsb = Wg_UnaryOp(WG_UOP_BOOL, lhs);
			if (lhsb == nullptr)
				return nullptr;
			if (Wg_GetBool(lhsb))
				return lhsb;
			return Wg_UnaryOp(WG_UOP_BOOL, rhs);
		}
		default:
			WUNREACHABLE();
		}
	}

} // extern "C"
