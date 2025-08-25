cmake_minimum_required(VERSION 3.18.1)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=${ARCH} -mabi=${ABI} -Wall -Werror")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")

project(init)

set(EXECUTABLE_NAME "${PROJECT_NAME}.elf")

enable_language(C)
include_directories(BEFORE ../include ../lib)
link_directories(${FS_LIB_BINARY_PATH})
add_executable(${EXECUTABLE_NAME} main.c cli.c icommands.c xmodem_server.c configure.c conffile.c peekpoke.c)
target_link_options(${EXECUTABLE_NAME} BEFORE PUBLIC -specs=${FS_LIB_SPECS_PATH}/filestick.specs )
