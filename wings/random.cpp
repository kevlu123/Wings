#include "random.h"
#include "common.h"

#include <random>

namespace wings {

	static constexpr const char* CODE = R"(
def choice(seq):
	t = tuple(seq)
	return t[randint(0, len(t) - 1)]

def getrandbits(n):
	x = 0
	for i in range(n):
		x <<= 1
		if random() < 0.5:
			x |= 1
	return x

def randrange(*args):
	return choice(range(*args))
		)";
	
	struct Rng {
		Rng() : engine(std::random_device()())
		{
		}

		void Seed(Wg_int seed) {
			engine.seed((unsigned long long)seed);
		}

		Wg_float Rand() {
			return dist(engine);
		}

		Wg_int Int(Wg_int minIncl, Wg_int maxIncl) {
			auto i = (Wg_int)((maxIncl - minIncl + 1) * Rand() + minIncl);
			
			if (i > maxIncl) // Just in case
				return maxIncl;

			return i;
		}

		Wg_float Float(Wg_float minIncl, Wg_float maxIncl) {
			return (maxIncl - minIncl) * Rand() + minIncl;
		}

		std::mt19937_64& Engine() {
			return engine;
		}

	private:
		std::mt19937_64 engine;
		std::uniform_real_distribution<Wg_float> dist;
	};

	static Wg_Obj* Rng_init(Wg_Context* context, Wg_Obj** argv, int argc) {
		Wg_SetUserdata(argv[0], new Rng());
		argv[0]->type = "__Rng";
		argv[0]->finalizer.fptr =[](Wg_Obj* obj, void*) { delete (Rng*)obj->data; };
		return Wg_None(context);
	}

	static Rng& GetGen(Wg_Context* context) {
		Wg_Obj* rng = Wg_GetGlobal(context, "__rng");
		Rng* gen{};
		TryGetUserdata(rng, "__Rng", &gen);
		return *gen;
	}

	static Wg_Obj* randint(Wg_Context* context, Wg_Obj** argv, int argc) {
		WG_EXPECT_ARG_COUNT(2);
		WG_EXPECT_ARG_TYPE_INT(0);
		WG_EXPECT_ARG_TYPE_INT(1);
		Wg_int lower = Wg_GetInt(argv[0]);
		Wg_int upper = Wg_GetInt(argv[1]);
		return Wg_NewInt(context, GetGen(context).Int(lower, upper));
	}

	static Wg_Obj* random(Wg_Context* context, Wg_Obj** argv, int argc) {
		WG_EXPECT_ARG_COUNT(0);
		return Wg_NewFloat(context, GetGen(context).Rand());
	}

	static Wg_Obj* seed(Wg_Context* context, Wg_Obj** argv, int argc) {
		WG_EXPECT_ARG_COUNT(1);
		WG_EXPECT_ARG_TYPE_INT(0);
		GetGen(context).Seed(Wg_GetInt(argv[0]));
		return Wg_None(context);
	}

	static Wg_Obj* shuffle(Wg_Context* context, Wg_Obj** argv, int argc) {
		WG_EXPECT_ARG_COUNT(1);
		WG_EXPECT_ARG_TYPE_LIST(0);
		auto& li = argv[0]->Get<std::vector<Wg_Obj*>>();
		std::shuffle(li.begin(), li.end(), GetGen(context).Engine());
		return Wg_None(context);
	}

	static Wg_Obj* uniform(Wg_Context* context, Wg_Obj** argv, int argc) {
		WG_EXPECT_ARG_COUNT(2);
		WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
		WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
		Wg_float lower = Wg_GetFloat(argv[0]);
		Wg_float upper = Wg_GetFloat(argv[1]);
		if (lower > upper) {
			Wg_RaiseException(context, WG_EXC_VALUEERROR, "Lower bound must be less than or equal to upper bound");
			return nullptr;
		}
		return Wg_NewFloat(context, GetGen(context).Float(lower, upper));
	}

	bool ImportRandom(Wg_Context* context) {
		Wg_Obj* rngClass = Wg_NewClass(context, "__Rng", nullptr, 0);
		if (rngClass == nullptr)
			return false;

		try {
		RegisterMethod(rngClass, "__init__", Rng_init);

			Wg_Obj* rng = Wg_Call(rngClass, nullptr, 0);
			if (rng == nullptr)
				throw LibraryInitException();
			Wg_SetGlobal(context, "__rng", rng);
		
			RegisterFunction(context, "seed", seed);
			RegisterFunction(context, "shuffle", shuffle);
			RegisterFunction(context, "randint", randint);
			RegisterFunction(context, "random", random);
			RegisterFunction(context, "uniform", uniform);

			if (!Execute(context, CODE, "random"))
				throw LibraryInitException();
			
			return true;
		} catch (LibraryInitException&) {
			return false;
		}
	}
}
