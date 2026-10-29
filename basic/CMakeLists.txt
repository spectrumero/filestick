cmake_minimum_required(VERSION 3.18.1)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=${ARCH} -mabi=${ABI} -Os -ffunction-sections -fdata-sections -Wall -Werror")
set(CMAKE_ASM_FLAGS "-march=${ARCH} -mabi=${ABI}")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")

project(basic)

set(EXECUTABLE_NAME "${PROJECT_NAME}.elf")

enable_language(C ASM)
include_directories(BEFORE ../include ../lib)
link_directories(${FS_LIB_BINARY_PATH})
add_executable(${EXECUTABLE_NAME} main.c eval.c stmt.c token.c num.c io.c stack.S)
target_link_options(${EXECUTABLE_NAME} BEFORE PUBLIC -specs=${FS_LIB_SPECS_PATH}/filestick.specs -Wl,--gc-sections)
