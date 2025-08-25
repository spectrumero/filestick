cmake_minimum_required(VERSION 3.18.1)

set(ARCH    "rv32imc")
set(ABI     "ilp32")
set(CMAKE_C_COMPILER "riscv-none-elf-gcc")

# Set paths to filestick.lib for this build.
set(FS_LIB_SPECS_PATH      ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(FS_LIB_BINARY_PATH     ${CMAKE_CURRENT_BINARY_DIR}/lib)

project(filestick)

add_subdirectory(lib)
add_subdirectory(boot)
add_subdirectory(system)
add_subdirectory(init)

