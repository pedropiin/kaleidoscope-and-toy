cmake_minimum_required(VERSION 3.13)

project(kaleidoscope)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

set(CMAKE_CXX_STANDARD 20)

add_executable(
    kaleidoscope 
    ./src/main.cpp 
    ./src/lexer.cpp 
    ./src/error.cpp
    ./src/ast.cpp 
    ./src/parser.cpp
    ./src/kaleidoscope_config.cpp
    ./src/codegen_visitor.cpp
)

# equivalent to 'llvm-config -cxxflags'
target_include_directories(kaleidoscope PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(kaleidoscope PRIVATE ${LLVM_DEFINITIONS})

# equivalent to 'llvm-config --libs <libnames_list...>'
llvm_map_components_to_libnames(
    LLVM_LIBS 
    core 
    support 
    passes 
    analysis 
    transformutils 
    instcombine 
    scalaropts
    native
    orcjit
)
target_link_libraries(kaleidoscope ${LLVM_LIBS})