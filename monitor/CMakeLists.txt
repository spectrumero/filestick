cmake_minimum_required(VERSION 3.18.1)

set(ARCH    "rv32imc")
set(ABI     "ilp32")
set(CMAKE_C_COMPILER "riscv-none-elf-gcc")

project(monitor)

set(EXECUTABLE_NAME "${PROJECT_NAME}.elf")

enable_language(C)
include_directories(BEFORE ../include ../lib)
add_executable(${EXECUTABLE_NAME} monitor.c decoder.c)
target_link_options(${EXECUTABLE_NAME} BEFORE PUBLIC -L../../build/lib -specs=../../build/lib/filestick.specs)

