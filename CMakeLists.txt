cmake_minimum_required(VERSION 3.23)
project(simple_JIT)

set(CMAKE_CXX_STANDARD 20)

add_executable(simple_JIT main.cpp recurrence.cpp recurrence.h)
