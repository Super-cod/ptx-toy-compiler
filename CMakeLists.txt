cmake_minimum_required(VERSION 3.20)
project(ptx_toy_compiler VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include_directories(include)

# Core compiler library
add_library(ptxcc_core STATIC
    src/Lexer.cpp
    src/AST.cpp
    src/Parser.cpp
    src/SemanticAnalyzer.cpp
    src/IR.cpp
    src/IRGenerator.cpp
    src/Optimizer.cpp
    src/PTXEmitter.cpp
    src/StackVM.cpp
)

# Compiler CLI executable
add_executable(ptxcc src/main.cpp)
target_link_libraries(ptxcc PRIVATE ptxcc_core)

# Unit and integration test executable
add_executable(ptxcc_tests tests/test_main.cpp)
target_link_libraries(ptxcc_tests PRIVATE ptxcc_core)

enable_testing()
add_test(NAME CompilerTests COMMAND ptxcc_tests)
