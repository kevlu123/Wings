#include "wings.h"
#include <iostream>
#include <string>

static std::string output;
static size_t testsPassed;
static size_t testsRun;

static void Expect(const char* code, const char* expected, size_t line) {
	testsRun++;
	output.clear();

	WContext* context{};
	try {
		context = WCreateContext();
		if (context == nullptr)
			throw std::string("Context creation failed");

		WConfig cfg{};
		WGetConfig(context, &cfg);
		cfg.print = [](const char* message, int len, void*) {
			output += std::string(message, len);
		};
		WSetConfig(context, &cfg);

		WObj* exe = WCompile(context, code);
		if (exe == nullptr)
			throw std::string(WGetErrorMessage(context));

		if (WCall(exe, nullptr, 0) == nullptr)
			throw std::string(WGetErrorMessage(context));

		std::string trimmed = output.substr(0, output.size() - 1);
		if (trimmed != std::string(expected)) {
			throw std::string("Test on line ")
				+ std::to_string(line)
				+ " failed. Expected "
				+ expected
				+ ". Got "
				+ trimmed
				+ ".";
		}

		testsPassed++;
	} catch (std::string& err) {
		std::cout << err << std::endl;
	}

	WDestroyContext(context);
}

static void ExpectFailure(const char* code, size_t line) {
	testsRun++;
	output.clear();
	
	WContext* context{};
	try {
		context = WCreateContext();
		if (context == nullptr)
			throw std::string("Context creation failed");

		WConfig cfg{};
		WGetConfig(context, &cfg);
		cfg.print = [](const char* message, int len, void*) {
			output += std::string(message, len);
		};
		WSetConfig(context, &cfg);

		WObj* exe = WCompile(context, code);
		if (exe == nullptr)
			throw std::string(WGetErrorMessage(context));

		if (WCall(exe, nullptr, 0) == nullptr)
			throw std::string(WGetErrorMessage(context));

		std::cout << "Test on line " << line << " did not fail as expected." << std::endl;
	} catch (std::string&) {
		testsPassed++;
	}

	WDestroyContext(context);
}

#define T(code, expected) Expect(code, expected, __LINE__)
#define F(code) ExpectFailure(code, __LINE__)

void TestPrint() {
	T("print(None)", "None");
	T("print(False)", "False");
	T("print(True)", "True");

	T("print(0)", "0");
	T("print(123)", "123");
	T("print(0b1101)", "13");
	T("print(017)", "15");
	T("print(0xfE)", "254");

	T("print(0.0)", "0.0");
	T("print(123.0)", "123.0");
	T("print(123.)", "123.0");
	T("print(0b1.1)", "1.5");
	T("print(01.2)", "1.25");
	T("print(0x1.2)", "1.125");

	T("print('')", "");
	T("print('hello')", "hello");
	T("print('\\tt')", "\tt");

	T("print(())", "()");
	T("print((0,))", "(0,)");
	T("print((0,1))", "(0, 1)");

	T("print([])", "[]");
	T("print([0])", "[0]");
	T("print([0,1])", "[0, 1]");

	T("print({})", "{}");
	T("print({0: 1})", "{0: 1}");

	T("x = []\nx.append(x)\nprint(x)", "[[...]]");

	T("print()", "");
	T("print(123, 'hello')", "123 hello");

	F("print(skdfjsl)");
}

void TestConditional() {
	T(R"(
if True:
	print(0)
else:
	print(1)
)"
,
"0"
);

	T(R"(
if False:
	print(0)
else:
	print(1)
)"
,
"1"
);

	T(R"(
if False:
	print(0)
elif False:
	print(1)
else:
	print(2)
)"
,
"2"
);

	T(R"(
if False:
	print(0)
elif True:
	print(1)
else:
	print(2)
)"
,
"1"
);

	T(R"(
if True:
	print(0)
elif False:
	print(1)
else:
	print(2)
)"
,
"0"
);

	T(R"(
if True:
	print(0)
elif True:
	print(1)
else:
	print(2)
)"
,
"0"
);

	T(R"(
if True:
	if True:
		print(0)
	else:
		print(1)
else:
	print(2)
)"
,
"0"
);
}

void TestWhile() {
	T(R"(
i = 0
while i < 10:
	i = i + 1
print(i)
)"
,
"10"
);

	T(R"(
i = 0
while i < 10:
	i = i + 1
else:
	i = None
print(i)
)"
,
"None"
);

	T(R"(
i = 0
while i < 10:
	i = i + 1
	break
else:
	i = None
print(i)
)"
,
"1"
);

	T(R"(
i = 0
while i < 10:
	i = i + 1
	continue
	break
else:
	i = None
print(i)
)"
,
"None"
);
}

void TestExceptions() {
	F(R"(
try:
	pass
)"
);

	F(R"(
except:
	pass
)"
);

	F(R"(
finally:
	pass
)"
);

	F(R"(
raise Exception
)"
);

	T(R"(
try:
	print("try")
except:
	print("except")
)"
,
"try"
);

	T(R"(
try:
	print("try")
	raise Exception
except:
	print("except")
)"
,
"try\nexcept"
);

	T(R"(
try:
	print("try")
except:
	print("except")
finally:
	print("finally")
)"
,
"try\nfinally"
);

	T(R"(
try:
	print("try")
	raise Exception
except:
	print("except")
finally:
	print("finally")
)"
,
"try\nexcept\nfinally"
);

	T(R"(
try:
	print("try")
finally:
	print("finally")
)"
,
"try\nfinally"
);

	T(R"(
try:
	print("try1")
	try:
		print("try2")
	except:
		print("except2")
	finally:
		print("finally2")
except:
	print("except1")
finally:
	print("finally1")
)"
,
"try1\ntry2\nfinally2\nfinally1"
);

	T(R"(
try:
	print("try1")
	try:
		print("try2")
		raise Exception
	except:
		print("except2")
		raise Exception
	finally:
		print("finally2")
except:
	print("except1")
finally:
	print("finally1")
)"
,
"try1\ntry2\nexcept2\nfinally2\nexcept1\nfinally1"
);

	T(R"(
try:
	print("try1")
	raise Exception
except:
	print("except1")
	try:
		print("try2")
		raise Exception
	except:
		print("except2")
	finally:
		print("finally2")
finally:
	print("finally1")
)"
,
"try1\nexcept1\ntry2\nexcept2\nfinally2\nfinally1"
);

	T(R"(
try:
	print("try1")
	raise Exception
except:
	print("except1")
	try:
		print("try2")
	except:
		print("except2")
	finally:
		print("finally2")
finally:
	print("finally1")
)"
,
"try1\nexcept1\ntry2\nfinally2\nfinally1"
);

	T(R"(
def f():
	raise Exception

try:
	print("try1")
	f()
except:
	print("except1")
	try:
		print("try2")
		f()
	except:
		print("except2")
	finally:
		print("finally2")
finally:
	print("finally1")
)"
,
"try1\nexcept1\ntry2\nexcept2\nfinally2\nfinally1"
);

	T(R"(
class Derived(Exception):
	pass

try:
	print("try")
	raise Exception("hello")
except Derived as e:
	print("except1", e)
except:
	print("except2")
finally:
	print("finally")
)"
,
"try\nexcept2\nfinally"
);

	T(R"(
class Derived(Exception):
	pass

try:
	print("try")
	raise Derived
except Derived as e:
	print("except1")
except:
	print("except2")
finally:
	print("finally")
)"
,
"try\nexcept1\nfinally"
);

	T(R"(
class Derived(Exception):
	pass

try:
	print("try")
	raise Derived
except Derived:
	print("except1")
except:
	print("except2")
finally:
	print("finally")
)"
,
"try\nexcept1\nfinally"
);
}

static void TestStringMethods() {
	T("print('abc'.capitalize())", "Abc");
	T("print('AbC'.casefold())", "abc");
	T("print('AbC'.lower())", "abc");
	T("print('AbC'.upper())", "ABC");
	T("print('AbC'.center(6, '-'))", "-AbC--");
	T("print('baaaa '.count('aa'))", "2");
	T("print('abc'.endswith('bc'))", "True");
	T("print('abc'.endswith('ab'))", "False");
	T("print('abc'.startswith('ab'))", "True");
	T("print('abc'.startswith('bc'))", "False");

	T("print('{},{}'.format(1, 2))", "1,2");
	T("print('{1},{0}'.format(1, 2))", "2,1");
	F("print('{0},{}'.format(1))");
	F("print('{1}'.format(1))");
	
	T("print('abc'.find('c'))", "2");
	T("print('abc'.find('d'))", "-1");
	T("print('abc'.find('c', 0, -1))", "-1");
	T("print('abc'.find('a', -1))", "-1");
	T("print('abc'.index('c'))", "2");
	F("print('abc'.index('c', 0, -1))");

	T("print('abcd01'.isalnum())", "True");
	T("print('abc!01'.isalnum())", "False");
	T("print('abcasa'.isalpha())", "True");
	T("print('abcv01'.isalpha())", "False");
	T("print('023413'.isdecimal())", "True");
	T("print('023a13'.isdecimal())", "False");
	T("print('a_23a1'.isidentifier())", "True");
	T("print('4_23a1'.isidentifier())", "False");
	T("print('4_2 a1'.isidentifier())", "False");
	T("print('9d98sf'.islower())", "True");
	T("print('93A09f'.islower())", "False");
	T("print('9D98SF'.isupper())", "True");
	T("print('93A09f'.isupper())", "False");
	T("print('      '.isspace())", "True");
	T("print('  s   '.isspace())", "False");
}

void RunTests() {
	TestPrint();
	TestConditional();
	TestWhile();
	TestExceptions();
	TestStringMethods();

	std::cout << testsPassed << "/" << testsRun << " tests passed." << std::endl << std::endl;
}
