#include "mathmodule.h"
#include "common.h"

#include <cmath>
#include <numbers>
#include <limits>

namespace wings {

	static constexpr const char* CODE = R"(
def comb(n, k):
	if not isinstance(n, int) or not isinstance(k, int):
		raise TypeError("comb() only accepts integers")
	if n < 0 or k < 0:
		raise ValueError("comb() only accepts non-negative integers")
	if k > n:
		return 0
	return factorial(n) // (factorial(k) * factorial(n - k))

def fabs(x):
	return float(abs(x))

def factorial(n):
	if not isinstance(n, int):
		raise TypeError("factorial() only accepts integers")
	if n < 0:
		raise ValueError("factorial() only accepts non-negative integers")
	if n == 0:
		return 1
	return n * factorial(n - 1)

def gcd(*integers):
	if len(integers) == 0:
		raise TypeError("gcd() requires at least one argument")
	for i in integers:
		if not isinstance(i, int):
			raise TypeError("gcd() only accepts integers")
	if len(integers) == 1:
		return abs(integers[0])
	if len(integers) == 2:
		(a, b) = integers
		if a == 0:
			return abs(b)
		if b == 0:
			return abs(a)
		while b != 0:
			(a, b) = (b, a % b)
		return abs(a)
	return gcd(gcd(integers[0], integers[1]), *integers[2:])

def lcm(*integers):
	if len(integers) == 0:
		raise TypeError("lcm() requires at least one argument")
	for i in integers:
		if not isinstance(i, int):
			raise TypeError("lcm() only accepts integers")
	if len(integers) == 1:
		return abs(integers[0])
	if len(integers) == 2:
		(a, b) = integers
		if a == 0 or b == 0:
			return 0
		return abs(a * b) // gcd(a, b)
	return lcm(lcm(integers[0], integers[1]), *integers[2:])

def modf(x):
	r = x % 1.0
	return (r, x - r)

def perm(n, k=None):
	if not isinstance(n, int):
		raise TypeError("perm() only accepts integers")
	if n < 0:
		raise ValueError("perm() only accepts non-negative integers")
	if k is None:
		k = n
	if not isinstance(k, int):
		raise TypeError("perm() only accepts integers")
	if k < 0:
		raise ValueError("perm() only accepts non-negative integers")
	if k > n:
		return 0
	return factorial(n) // factorial(n - k)

def trunc(x):
	if x >= 0:
		return int(x)
	return int(x) - 1

def exp(x):
	return e ** x

def log1p(x):
	return log(1 + x)

def log2(x):
	return log(x, 2)

def log10(x):
	return log(x, 10)

def pow(x, y):
	if x == 1 or y == 0:
		return 1
	if isfinite(x) and isfinite(y) and x < 0 and isinstance(y, int):
		raise ValueError("negative number cannot be raised to a fractional power")
	return x ** y

def sqrt(x):
	return x ** 0.5

def dist(p, q):
	return sqrt(sum([(z[0] - z[1]) ** 2 for z in zip(p, q)]))

def hypot(*coords):
	return sqrt(sum([x ** 2 for x in coords]))

def degrees(x):
	return x * 180.0 / pi

def radians(x):
	return x * pi / 180.0
)";
	
	static Wg_Obj* ceil(Wg_Context* context, Wg_Obj** argv, int argc) {
		WG_EXPECT_ARG_COUNT(1);
		if (Wg_IsIntOrFloat(argv[0])) {
			return Wg_NewInt(context, (Wg_int)std::ceil(Wg_GetFloat(argv[0])));
		}
		return Wg_CallMethod(argv[0], "__ceil__", nullptr, 0);
	}

	static Wg_Obj* floor(Wg_Context* context, Wg_Obj** argv, int argc) {
		WG_EXPECT_ARG_COUNT(1);
		if (Wg_IsIntOrFloat(argv[0])) {
			return Wg_NewInt(context, (Wg_int)std::floor(Wg_GetFloat(argv[0])));
		}
		return Wg_CallMethod(argv[0], "__floor__", nullptr, 0);
	}

	using FpCheck = bool(*)(Wg_float);

	template <FpCheck f>
	static Wg_Obj* isx(Wg_Context* context, Wg_Obj** argv, int argc) {
		WG_EXPECT_ARG_COUNT(1);
		WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
		return Wg_NewBool(context, f(Wg_GetFloat(argv[0])));
	}

	static Wg_Obj* isfinite(Wg_Context* context, Wg_Obj** argv, int argc) {
		return isx<std::isfinite>(context, argv, argc);
	}

	static Wg_Obj* isinf(Wg_Context* context, Wg_Obj** argv, int argc) {
		return isx<std::isinf>(context, argv, argc);
	}

	static Wg_Obj* isnan(Wg_Context* context, Wg_Obj** argv, int argc) {
		return isx<std::isnan>(context, argv, argc);
	}

	static Wg_Obj* log(Wg_Context* context, Wg_Obj** argv, int argc) {
		WG_EXPECT_ARG_COUNT_BETWEEN(1, 2);
		WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
		Wg_float base = std::numbers::e_v<Wg_float>;
		if (argc == 2) {
			WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
			base = Wg_GetFloat(argv[1]);
		}
		return Wg_NewFloat(context, std::log(Wg_GetFloat(argv[0])) / std::log(base));
	}

	using Op = Wg_float(*)(Wg_float);

	template <Op op>
	static Wg_Obj* opx(Wg_Context* context, Wg_Obj** argv, int argc) {
		WG_EXPECT_ARG_COUNT(1);
		WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
		return Wg_NewFloat(context, op(Wg_GetFloat(argv[0])));
	}

	static Wg_Obj* cos(Wg_Context* context, Wg_Obj** argv, int argc) {
		return opx<std::cos>(context, argv, argc);
	}

	static Wg_Obj* sin(Wg_Context* context, Wg_Obj** argv, int argc) {
		return opx<std::sin>(context, argv, argc);
	}

	static Wg_Obj* tan(Wg_Context* context, Wg_Obj** argv, int argc) {
		return opx<std::tan>(context, argv, argc);
	}

	static Wg_Obj* acos(Wg_Context* context, Wg_Obj** argv, int argc) {
		return opx<std::acos>(context, argv, argc);
	}

	static Wg_Obj* asin(Wg_Context* context, Wg_Obj** argv, int argc) {
		return opx<std::asin>(context, argv, argc);
	}

	static Wg_Obj* atan(Wg_Context* context, Wg_Obj** argv, int argc) {
		return opx<std::atan>(context, argv, argc);
	}

	static Wg_Obj* cosh(Wg_Context* context, Wg_Obj** argv, int argc) {
		return opx<std::cosh>(context, argv, argc);
	}

	static Wg_Obj* sinh(Wg_Context* context, Wg_Obj** argv, int argc) {
		return opx<std::sinh>(context, argv, argc);
	}

	static Wg_Obj* tanh(Wg_Context* context, Wg_Obj** argv, int argc) {
		return opx<std::tanh>(context, argv, argc);
	}

	static Wg_Obj* acosh(Wg_Context* context, Wg_Obj** argv, int argc) {
		return opx<std::acosh>(context, argv, argc);
	}

	static Wg_Obj* asinh(Wg_Context* context, Wg_Obj** argv, int argc) {
		return opx<std::asinh>(context, argv, argc);
	}

	static Wg_Obj* atanh(Wg_Context* context, Wg_Obj** argv, int argc) {
		return opx<std::atanh>(context, argv, argc);
	}

	static Wg_Obj* erf(Wg_Context* context, Wg_Obj** argv, int argc) {
		return opx<std::erf>(context, argv, argc);
	}

	static Wg_Obj* erfc(Wg_Context* context, Wg_Obj** argv, int argc) {
		return opx<std::erfc>(context, argv, argc);
	}

	static Wg_Obj* gamma(Wg_Context* context, Wg_Obj** argv, int argc) {
		return opx<std::tgamma>(context, argv, argc);
	}

	static Wg_Obj* lgamma(Wg_Context* context, Wg_Obj** argv, int argc) {
		return opx<std::lgamma>(context, argv, argc);
	}

	static Wg_Obj* atan2(Wg_Context* context, Wg_Obj** argv, int argc) {
		WG_EXPECT_ARG_COUNT(2);
		WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(0);
		WG_EXPECT_ARG_TYPE_INT_OR_FLOAT(1);
		return Wg_NewFloat(context, std::atan2(Wg_GetFloat(argv[0]), Wg_GetFloat(argv[1])));
	}
	
	bool ImportMath(Wg_Context* context) {
		try {
			RegisterFunction(context, "ceil", ceil);
			RegisterFunction(context, "floor", floor);
			RegisterFunction(context, "isfinite", isfinite);
			RegisterFunction(context, "isinf", isinf);
			RegisterFunction(context, "isnan", isnan);
			RegisterFunction(context, "log", log);
			RegisterFunction(context, "cos", cos);
			RegisterFunction(context, "sin", sin);
			RegisterFunction(context, "tan", tan);
			RegisterFunction(context, "acos", acos);
			RegisterFunction(context, "asin", asin);
			RegisterFunction(context, "atan", atan);
			RegisterFunction(context, "atan2", atan2);
			RegisterFunction(context, "cosh", cosh);
			RegisterFunction(context, "sinh", sinh);
			RegisterFunction(context, "tanh", tanh);
			RegisterFunction(context, "acosh", acosh);
			RegisterFunction(context, "asinh", asinh);
			RegisterFunction(context, "atanh", atanh);
			RegisterFunction(context, "erf", erf);
			RegisterFunction(context, "erfc", erfc);
			RegisterFunction(context, "gamma", gamma);
			RegisterFunction(context, "lgamma", lgamma);

			RegisterConstant(context, "e", Wg_NewFloat, std::numbers::e_v<Wg_float>);
			RegisterConstant(context, "inf", Wg_NewFloat, std::numeric_limits<Wg_float>::infinity());
			RegisterConstant(context, "nan", Wg_NewFloat, std::numeric_limits<Wg_float>::quiet_NaN());
			RegisterConstant(context, "pi", Wg_NewFloat, std::numbers::pi_v<Wg_float>);
			RegisterConstant(context, "tau", Wg_NewFloat, 2 * std::numbers::pi_v<Wg_float>);

			if (Execute(context, CODE, "math") == nullptr)
				throw LibraryInitException();

			return true;
		} catch (LibraryInitException&) {
			return false;
		}
	}
}
