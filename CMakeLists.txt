cmake_minimum_required(VERSION 3.16)
project(jnp1_kvfifo)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-Wfatal-errors -Wall -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG  "-D_GLIBCXX_DEBUG")

include_directories(.)

add_executable(jnp1_kvfifo
        counter.h
        kvfifo.h
        kvfifo.cc
        kvfifo_example.cc
        )

add_custom_target(format
        COMMAND /usr/bin/clang-format
        -i *
        )