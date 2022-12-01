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
