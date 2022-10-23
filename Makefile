PY=python3
CXX=g++
CXX_FLAGS=-O3 -std=c++20
BUILD_DIR=build

SHELL_TARGET=wings
SHELL_SRC=shell/main.cpp
SHELL_INCLUDES=-I$(BUILD_DIR)

DEV_SRC= \
	attributetable.cpp \
	osmodule.cpp       \
	sysmodule.cpp      \
	builtinsmodule.cpp \
	dismodule.cpp      \
	lex.cpp            \
	parse.cpp          \
	tests.cpp          \
	common.cpp         \
	executor.cpp       \
	mathmodule.cpp     \
	randommodule.cpp   \
	timemodule.cpp     \
	compile.cpp        \
	exprparse.cpp      \
	wings.cpp          \
	main.cpp
DEV_OBJ=$(DEV_SRC:%.cpp=$(DEV_OBJ_DIR)/%.o)
DEV_SRC_DIR=wings
DEV_OBJ_DIR=wings/obj
DEV_TARGET=dev

# Build all projects
all: dev shell

# Build the standalone shell
shell: dev
	@echo Building $(SHELL_TARGET)
	@$(CXX) $(CXX_FLAGS) $(SHELL_INCLUDES) -o $(BUILD_DIR)/$(SHELL_TARGET) $(SHELL_SRC)
	
# Build the development project containing the unmerged source.
dev: directories $(DEV_OBJ)
	@echo Linking $(DEV_TARGET)
	@$(CXX) $(CXX_FLAGS) -o $(BUILD_DIR)/$(DEV_TARGET) $(DEV_OBJ)
	@$(PY) wings/merge.py

$(DEV_OBJ):
	@echo Compiling $@
	@$(CXX) -c $(CXX_FLAGS) -o $@ $(@:%.o=$(DEV_SRC_DIR)/$(notdir $(@:%.o=%.cpp)))

# Create output directories
directories:
	@echo Creating output directories
	@mkdir -p $(DEV_OBJ_DIR)
	@mkdir -p $(BUILD_DIR)

.PHONY: clean
clean:
	@-rm -f $(DEV_OBJ_DIR)/*
	@-rm -rf $(BUILD_DIR)/*
