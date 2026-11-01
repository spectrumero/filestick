cmake_minimum_required(VERSION 3.18.1)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=${ARCH} -mabi=${ABI} -Os -Wall -Werror")
set(CMAKE_ASM_FLAGS "-march=${ARCH} -mabi=${ABI}")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")

project(cputest)

set(EXECUTABLE_NAME "${PROJECT_NAME}.elf")

enable_language(C ASM)
include_directories(BEFORE ../include ../lib)
link_directories(${FS_LIB_BINARY_PATH})
add_executable(${EXECUTABLE_NAME} main.c framework.S rv32i_alu.S rv32i_mem.S
               rv32i_ctrl.S rv32m.S rv32c.S fence.S)
target_link_options(${EXECUTABLE_NAME} BEFORE PUBLIC -specs=${FS_LIB_SPECS_PATH}/filestick.specs)
