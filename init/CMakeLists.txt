cmake_minimum_required(VERSION 3.18.1)

set(ARCH    "rv32imc")
set(ABI     "ilp32")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=${ARCH} -mabi=${ABI} -Wall -Werror")
set(CMAKE_C_COMPILER "riscv-none-elf-gcc")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")

project(init)

set(EXECUTABLE_NAME "${PROJECT_NAME}.elf")

enable_language(C)
include_directories(BEFORE ../include ../lib)
#link_directories(lib)
add_executable(${EXECUTABLE_NAME} main.c cli.c icommands.c)
target_link_options(${EXECUTABLE_NAME} BEFORE PUBLIC -L../../build/lib -specs=../../build/lib/filestick.specs )
