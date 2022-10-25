#include <iostream>
#include <fstream>
#include <vector>

#include "wings.h"
#include "tests.h"

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

int main(int argc, char** argv) {
	// If test.py exists, run it.
	// The purpose of this is to make it easier to perform quick, temporary tests.
	std::vector<char> script;
	if (ReadFromFile("test.py", script)) {
		// Add null terminator
		script.push_back(0);

		Wg_Config cfg{};
		Wg_DefaultConfig(&cfg);
		cfg.argc = argc;
		cfg.argv = argv;
		cfg.enableOSAccess = true;
		
		if (Wg_Context* context = Wg_CreateContext(&cfg)) {
			if (!Wg_Execute(context, script.data(), "test.py")) {
				std::cout << Wg_GetErrorMessage(context) << std::endl;
			}
			Wg_DestroyContext(context);
		} else {
			std::cout << "Failed to initialise context for test.py" << std::endl;
		}
	}
	
	wings::RunTests();
	return 0;
}
