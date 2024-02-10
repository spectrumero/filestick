cmake_minimum_required(VERSION 3.18.1)

project(simlib)
set(LIBRARY_NAME "${PROJECT_NAME}")

enable_language(C)
include_directories(BEFORE ../include)
add_library(${LIBRARY_NAME} STATIC udp_econet.c)

