cmake_minimum_required(VERSION 3.13)

project(toy)

find_package(LLVM REQUIRED CONFIG)

set(CMAKE_CXX_STANDARD 20)
set(LLVM_LINK_COMPONENTS
    Support
)

add_executable(
    toy
    main.cpp
    ast.cpp
)

include_directories(include/)
target_link_libraries(toy
    PRIVATE
        MLIRSupport
)