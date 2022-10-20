#include "randommodule.h"
#include "common.h"

#include <random>

namespace wings {

	static constexpr const char* RAND_CODE = R"(
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
	
	static Wg_Obj* randint(Wg_Context* context, Wg_Obj** argv, int argc) {
		WG_EXPECT_ARG_COUNT(2);
		WG_EXPECT_ARG_TYPE_INT(0);
		WG_EXPECT_ARG_TYPE_INT(1);
		Wg_int lower = Wg_GetInt(argv[0]);
		Wg_int upper = Wg_GetInt(argv[1]);
		return Wg_NewInt(context, context->rng.Int(lower, upper));
	}

	static Wg_Obj* random(Wg_Context* context, Wg_Obj** argv, int argc) {
		WG_EXPECT_ARG_COUNT(0);
		return Wg_NewFloat(context, context->rng.Rand());
	}

	static Wg_Obj* seed(Wg_Context* context, Wg_Obj** argv, int argc) {
		WG_EXPECT_ARG_COUNT(1);
		WG_EXPECT_ARG_TYPE_INT(0);
		context->rng.Seed(Wg_GetInt(argv[0]));
		return Wg_None(context);
	}

	static Wg_Obj* shuffle(Wg_Context* context, Wg_Obj** argv, int argc) {
		WG_EXPECT_ARG_COUNT(1);
		WG_EXPECT_ARG_TYPE_LIST(0);
		auto& li = argv[0]->Get<std::vector<Wg_Obj*>>();
		std::shuffle(li.begin(), li.end(), context->rng.Engine());
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
		return Wg_NewFloat(context, context->rng.Float(lower, upper));
	}

	bool ImportRandom(Wg_Context* context) {
		Wg_Obj* rngClass = Wg_NewClass(context, "__Rng", nullptr, 0);
		if (rngClass == nullptr)
			return false;

		try {
			RegisterFunction(context, "seed", seed);
			RegisterFunction(context, "shuffle", shuffle);
			RegisterFunction(context, "randint", randint);
			RegisterFunction(context, "random", random);
			RegisterFunction(context, "uniform", uniform);

			if (!Execute(context, RAND_CODE, "random"))
				throw LibraryInitException();
			
			return true;
		} catch (LibraryInitException&) {
			return false;
		}
	}
}
