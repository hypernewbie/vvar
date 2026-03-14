cmake_minimum_required(VERSION 3.20)
project(vvar LANGUAGES CXX)

# Detect Ninja generator
if(CMAKE_GENERATOR STREQUAL "Ninja")
    message(STATUS "Using Ninja generator")
endif()

# Detect MSVC/clang-cl or other compilers
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /W4 /permissive-")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
endif()

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Main library target (header-only)
add_library(vvar INTERFACE)
target_include_directories(vvar INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Tests
enable_testing()
add_executable(vvar_tests tests/vvar_tests.cpp)
target_link_libraries(vvar_tests PRIVATE vvar)

if(MSVC)
    target_compile_options(vvar_tests PRIVATE /EHsc /W4 /permissive-)
    target_link_options(vvar_tests PRIVATE /SUBSYSTEM:CONSOLE)
else()
    target_compile_options(vvar_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME vvar_tests COMMAND vvar_tests)
