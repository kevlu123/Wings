#include "osmodule.h"
#include "common.h"

#include <cstdlib>
#include <filesystem>

namespace wings {
	namespace fs = std::filesystem;

	static Wg_Obj* system(Wg_Context* context, Wg_Obj** argv, int argc) {
		WG_EXPECT_ARG_COUNT(1);
		WG_EXPECT_ARG_TYPE_STRING(0);
		int ec = std::system(Wg_GetString(argv[0]));
		return Wg_NewInt(context, (Wg_int)ec);
	}

	static Wg_Obj* mkdir(Wg_Context* context, Wg_Obj** argv, int argc) {
		WG_EXPECT_ARG_COUNT(1);
		WG_EXPECT_ARG_TYPE_STRING(0);
		const char* path = Wg_GetString(argv[0]);
		std::error_code ec{};
		if (!fs::create_directory(path, ec)) {
			Wg_RaiseException(context, WG_EXC_OSERROR);
			return nullptr;
		}
		return Wg_None(context);
	}

	static Wg_Obj* makedirs(Wg_Context* context, Wg_Obj** argv, int argc) {
		WG_EXPECT_ARG_COUNT(1);
		WG_EXPECT_ARG_TYPE_STRING(0);
		const char* path = Wg_GetString(argv[0]);
		std::error_code ec{};
		if (!fs::create_directories(path, ec)) {
			Wg_RaiseException(context, WG_EXC_OSERROR);
			return nullptr;
		}
		return Wg_None(context);
	}

	static Wg_Obj* remove(Wg_Context* context, Wg_Obj** argv, int argc) {
		WG_EXPECT_ARG_COUNT(1);
		WG_EXPECT_ARG_TYPE_STRING(0);
		const char* path = Wg_GetString(argv[0]);
		
		std::error_code ec{};
		if (!fs::is_regular_file(path, ec)) {
			Wg_RaiseException(context, WG_EXC_ISADIRECTORYERROR);
			return nullptr;
		}
		
		if (!fs::remove(path, ec)) {
			Wg_RaiseException(context, WG_EXC_OSERROR);
			return nullptr;
		}
		
		return Wg_None(context);
	}

	static Wg_Obj* rmdir(Wg_Context* context, Wg_Obj** argv, int argc) {
		WG_EXPECT_ARG_COUNT(1);
		WG_EXPECT_ARG_TYPE_STRING(0);
		const char* path = Wg_GetString(argv[0]);

		std::error_code ec{};
		if (!fs::is_directory(path, ec)) {
			Wg_RaiseException(context, WG_EXC_OSERROR);
			return nullptr;
		}

		if (!fs::remove(path, ec)) {
			Wg_RaiseException(context, WG_EXC_OSERROR);
			return nullptr;
		}

		return Wg_None(context);
	}

	static Wg_Obj* rename(Wg_Context* context, Wg_Obj** argv, int argc) {
		WG_EXPECT_ARG_COUNT(2);
		WG_EXPECT_ARG_TYPE_STRING(0);
		WG_EXPECT_ARG_TYPE_STRING(1);
		const char* src = Wg_GetString(argv[0]);
		const char* dst = Wg_GetString(argv[1]);

		std::error_code ec{};
		fs::rename(src, dst, ec);
		if (ec) {
			Wg_RaiseException(context, WG_EXC_OSERROR);
			return nullptr;
		}

		return Wg_None(context);
	}

	static Wg_Obj* listdir(Wg_Context* context, Wg_Obj** argv, int argc) {
		WG_EXPECT_ARG_COUNT_BETWEEN(0, 1);
		const char* path = ".";
		if (argc == 1) {
			WG_EXPECT_ARG_TYPE_STRING(0);
			path = Wg_GetString(argv[0]);
		}

		std::error_code ec{};
		std::vector<fs::path> paths;
		for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
			paths.push_back(entry.path());
		}

		if (ec) {		
			Wg_RaiseException(context, WG_EXC_OSERROR);
			return nullptr;
		}
		
		Wg_Obj* list = Wg_NewList(context);
		if (list == nullptr)
			return nullptr;
		Wg_ObjRef ref(list);
		
		for (const auto& path : paths) {
			Wg_Obj* entry = Wg_NewString(context, path.string().c_str());
			if (entry == nullptr)
				return nullptr;
			if (Wg_CallMethod(list, "append", &entry, 1) == nullptr)
				return nullptr;
		}

		return list;
	}

	static Wg_Obj* abort(Wg_Context* context, Wg_Obj** argv, int argc) {
		WG_EXPECT_ARG_COUNT(0);
		std::abort();
	}

	static Wg_Obj* chdir(Wg_Context* context, Wg_Obj** argv, int argc) {
		WG_EXPECT_ARG_COUNT(1);
		WG_EXPECT_ARG_TYPE_STRING(0);
		const char* path = Wg_GetString(argv[0]);

		std::error_code ec{};
		fs::current_path(path, ec);
		if (ec) {
			Wg_RaiseException(context, WG_EXC_OSERROR);
			return nullptr;
		}

		return Wg_None(context);
	}

	static Wg_Obj* getcwd(Wg_Context* context, Wg_Obj** argv, int argc) {
		WG_EXPECT_ARG_COUNT(0);
		auto path = fs::current_path().string();
		return Wg_NewString(context, path.c_str());
	}

	bool ImportOs(Wg_Context* context) {
		try {
			RegisterFunction(context, "system", system);
			RegisterFunction(context, "chdir", chdir);
			RegisterFunction(context, "getcwd", getcwd);
			RegisterFunction(context, "mkdir", mkdir);
			RegisterFunction(context, "makedirs", makedirs);
			RegisterFunction(context, "remove", remove);
			RegisterFunction(context, "rmdir", rmdir);
			RegisterFunction(context, "rename", rename);
			RegisterFunction(context, "listdir", listdir);
			RegisterFunction(context, "abort", abort);
			
			Wg_SetGlobal(context, "error", Wg_GetGlobal(context, "OSError"));

#ifdef _WIN32
			RegisterConstant(context, "sep", Wg_NewString, "\\");
			RegisterConstant(context, "linesep", Wg_NewString, "\r\n");
#else
			RegisterConstant(context, "sep", Wg_NewString, "/");
			RegisterConstant(context, "linesep", Wg_NewString, "\r");
#endif
			return true;
		} catch (LibraryInitException&) {
			return false;
		}
	}
}
