cmake_minimum_required(VERSION 3.18.1)

set(CMAKE_ASM_FLAGS  "-march=${ARCH}")

project(filestick)
set(LIBRARY_NAME "${PROJECT_NAME}")

enable_language(C ASM)
include_directories(BEFORE ../include)
add_library(${LIBRARY_NAME} STATIC sbrk.c ioctl.c)

