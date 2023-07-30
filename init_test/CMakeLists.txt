cmake_minimum_required(VERSION 3.18.1)

set(LINKER_SCRIPT    "init_test.ld")
set(CMAKE_ASM_FLAGS  "-march=${ARCH}")
set(CMAKE_EXE_LINKER_FLAGS "-T ${CMAKE_CURRENT_SOURCE_DIR}/${LINKER_SCRIPT}")
set(CMAKE_C_LINK_EXECUTABLE "<CMAKE_LINKER> <LINK_FLAGS> -o <TARGET> <OBJECTS>")

project(filestick-test)
set(EXECUTABLE_NAME "${PROJECT_NAME}.elf")

enable_language(C ASM)
add_executable(${EXECUTABLE_NAME} lib.s test.c strlen.c)
add_custom_target(init_test_bin ALL DEPENDS ${PROJECT_NAME}.bin)
add_custom_command(
   OUTPUT ${PROJECT_NAME}.bin
   DEPENDS ${EXECUTABLE_NAME}
   POST_BUILD
   COMMAND "${CMAKE_OBJCOPY}"
   ARGS ${EXECUTABLE_NAME} -O binary ${PROJECT_NAME}.bin
   )
set_target_properties(${TARGET_NAME} PROPERTIES LINK_DEPENDS ${LINKER_SCRIPT})

