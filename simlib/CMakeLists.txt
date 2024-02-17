cmake_minimum_required(VERSION 3.18.1)

set(CMAKE_C_FLAGS    "-DSIMULATOR -DSIMLIB")

project(simlib)
set(LIBRARY_NAME "${PROJECT_NAME}")

enable_language(C)
include_directories(BEFORE ../include ../system)
add_library(${LIBRARY_NAME} STATIC simulator.c sim_syscalls.c udp_econet.c ../system/fd.c ../system/dev_open.c sim_console.c sim_flashdev.c sim_econet.c sim_file.c sim_rgbled.c)

