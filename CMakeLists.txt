cmake_minimum_required(VERSION 3.23)
project(simple_JIT)

set(CMAKE_CXX_STANDARD 20)

add_executable(simple_JIT main.cpp recurrence.cpp recurrence.h)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNDEBUG -Ofast")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb3 -O0")# -fsanitize=thread,undefined)
endif()