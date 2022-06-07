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
		if (!WCreateContext(&context))
			throw std::string(WGetErrorMessage(context));

		WConfig cfg{};
		WGetConfig(context, &cfg);
		cfg.print = [](const char* message, int len) {
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
		if (!WCreateContext(&context))
			throw std::string(WGetErrorMessage(context));

		WConfig cfg{};
		WGetConfig(context, &cfg);
		cfg.print = [](const char* message, int len) {
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

void TestControlFlow() {
	T(
		"if True:\n"
		" print(0)\n"
		"else:\n"
		" print(1)\n",

		"0"
	);

	T(
		"if False:\n"
		" print(0)\n"
		"else:\n"
		" print(1)\n",

		"1"
	);
}

void RunTests() {
	TestPrint();
	TestControlFlow();

	std::cout << testsPassed << "/" << testsRun << " tests passed." << std::endl << std::endl;
}
