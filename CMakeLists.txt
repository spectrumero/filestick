cmake_minimum_required(VERSION 3.18.1)

set(ARCH    "rv32ic")
set(ABI     "ilp32")
set(CMAKE_C_COMPILER "riscv-none-elf-gcc")

project(filestick)

add_subdirectory(boot)
add_subdirectory(system)
add_subdirectory(init_test)

