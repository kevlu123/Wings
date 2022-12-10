#define WINGS_IMPL
#include "wings.h"

#include <stdint.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <stack>
#include <string.h>

static bool ReadFromFile(const std::string& path, std::vector<char>& data) {
	std::ifstream f(path, std::ios::binary | std::ios::ate);
	if (!f.is_open())
		return false;
	size_t len = f.tellg();
	f.seekg(0);
	data.resize(len);
	f.read(data.data(), len);
	return (bool)f;
}

int RunFile(int argc, char** argv) {
	std::vector<char> script;
	if (!ReadFromFile(argv[1], script))
		return 1;
	script.push_back(0); // Add null terminator

	Wg_Config cfg{};
	Wg_DefaultConfig(&cfg);
	cfg.argc = argc - 1;
	cfg.argv = argv + 1;
	cfg.enableOSAccess = true;
	cfg.importPath = argv[1];

	Wg_Context* context = Wg_CreateContext(&cfg);
	if (context == nullptr) {
		return 2;
	}

	Wg_Execute(context, script.data(), argv[1]);
	Wg_DestroyContext(context);
	return 0;
}

void PrintVersion() {
#ifdef WINGS_SHELL_VERSION
#define STRINGIFY(s) #s
#define XSTRINGIFY(s) STRINGIFY(s)
	std::cout << "Wings Shell v" XSTRINGIFY(WINGS_SHELL_VERSION) << std::endl;
#else
	std::cout << "Wings Shell" << std::endl;
#endif
}

int RunRepl() {
	Wg_Config cfg{};
	Wg_DefaultConfig(&cfg);
	cfg.enableOSAccess = true;
	
	Wg_Context* context = Wg_CreateContext(&cfg);
	if (context == nullptr) {
		return 1;
	}

	Wg_Obj* sysexit = Wg_GetGlobal(context, "SystemExit");
	Wg_IncRef(sysexit);

	// This context is only used to check if strings
	// are expressions rather than a set of statements.
	Wg_Context* exprChecker = Wg_CreateContext();
	if (exprChecker == nullptr) {
		Wg_DestroyContext(context);
		return 2;
	}

	PrintVersion();
	
	std::string input;
	bool indented = false;
	while (true) {
		if (input.empty()) {
			std::cout << ">>> ";
		} else {
			std::cout << "... ";
		}
		
		std::string line;
		std::getline(std::cin, line);
		input += line + "\n";

		size_t lastCharIndex = line.find_last_not_of(" \t");
		if (lastCharIndex != std::string::npos && line[lastCharIndex] == ':') {
			indented = true;
			continue;
		}

		if (indented && !line.empty()) {
			continue;
		}
		
		Wg_Obj* result = nullptr;
		Wg_ClearException(exprChecker);
		if (Wg_CompileExpression(exprChecker, input.c_str())) {
			result = Wg_ExecuteExpression(context, input.c_str(), "<string>");
		} else {
			Wg_Execute(context, input.c_str(), "<string>");
		}
		input.clear();
		indented = false;
		
		if (result && !Wg_IsNone(result)) {
			if (Wg_Obj* repr = Wg_UnaryOp(WG_UOP_REPR, result)) {
				std::cout << Wg_GetString(repr) << std::endl;
			}
		}
		
		Wg_Obj* exc = Wg_GetException(context);
		if (exc) {
			if (Wg_IsInstance(exc, &sysexit, 1)) {
				break;
			}
			
			std::cout << Wg_GetErrorMessage(context);
			Wg_ClearException(context);
		}
	}

	Wg_DestroyContext(exprChecker);
	Wg_DestroyContext(context);
	return 0;
}

int main(int argc, char** argv) {
	if (argc <= 1) {
		return RunRepl();
	}

	if (std::strcmp(argv[1], "--version") == 0) {
		PrintVersion();
		return 0;
	}

	return RunFile(argc, argv);
}
