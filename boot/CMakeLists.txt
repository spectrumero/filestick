cmake_minimum_required(VERSION 3.18.1)

set(LINKER_SCRIPT    "stage0.ld")
set(CMAKE_ASM_FLAGS  "-march=${ARCH}")
set(CMAKE_EXE_LINKER_FLAGS "-T ${CMAKE_CURRENT_SOURCE_DIR}/${LINKER_SCRIPT}")
set(CMAKE_ASM_LINK_EXECUTABLE "<CMAKE_LINKER> <LINK_FLAGS> -o <TARGET> <OBJECTS>")

project(filestick-boot)
set(EXECUTABLE_NAME "${PROJECT_NAME}.elf")

enable_language(ASM)
add_executable(${EXECUTABLE_NAME} stage0.s serial_putc.s serial_write.s spi_flash.s)
add_custom_target(bin ALL DEPENDS ${PROJECT_NAME}.bin)
add_custom_command(
   OUTPUT ${PROJECT_NAME}.bin
   DEPENDS ${EXECUTABLE_NAME}
   POST_BUILD
   COMMAND "${CMAKE_OBJCOPY}"
   ARGS ${EXECUTABLE_NAME} -O binary ${PROJECT_NAME}.bin
   )
set_target_properties(${TARGET_NAME} PROPERTIES LINK_DEPENDS ${LINKER_SCRIPT})

