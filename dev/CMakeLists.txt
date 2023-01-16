cmake_minimum_required(VERSION 3.0)

set(CMAKE_CXX_STANDARD 20)

add_executable(dev
    attributetable.cpp attributetable.h
    builtinsmodule.cpp builtinsmodule.h
    common.cpp common.h
    compile.cpp compile.h
    dismodule.cpp dismodule.h
    executor.cpp executor.h
    exprparse.cpp exprparse.h
    hash.h
    lex.cpp lex.h
    main.cpp
    mathmodule.cpp mathmodule.h
    osmodule.cpp osmodule.h
    parse.cpp parse.h
    randommodule.cpp randommodule.h
    rcptr.h
    sysmodule.cpp sysmodule.h
    tests.cpp tests.h
    timemodule.cpp timemodule.h
    wings.cpp wings.h
    )

IF(WIN32)
    set(PYTHON_EXECUTABLE python)
ELSE()
    set(PYTHON_EXECUTABLE python3)
ENDIF()

add_custom_target(
    merge ALL
    COMMAND ${PYTHON_EXECUTABLE} "${PROJECT_SOURCE_DIR}/scripts/merge.py"
)

add_custom_target(
    cs_bindgen ALL
    COMMAND ${PYTHON_EXECUTABLE} "${PROJECT_SOURCE_DIR}/scripts/cs_bindgen.py"
)

add_dependencies(dev merge)
add_dependencies(dev cs_bindgen)
