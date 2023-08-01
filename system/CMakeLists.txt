cmake_minimum_required(VERSION 3.18.1)

set(LINKER_SCRIPT    "system.ld")
set(CMAKE_ASM_FLAGS  "-march=${ARCH}")
set(CMAKE_EXE_LINKER_FLAGS "-T ${CMAKE_CURRENT_SOURCE_DIR}/${LINKER_SCRIPT}")
set(CMAKE_C_LINK_EXECUTABLE "<CMAKE_LINKER> <LINK_FLAGS> -o <TARGET> <OBJECTS>")

project(filestick-system)
set(EXECUTABLE_NAME "${PROJECT_NAME}.elf")

enable_language(C ASM)
add_executable(${EXECUTABLE_NAME} init.s super_trap.s isr_trap.s timer.s console_rx.s serial_putc.s spi_flash.s fd.c dev_open.c memset.c console.c strncmp.c strcmp.c rgbled.c brk.c exit.c spi_flashdev.c elfload.c strlen.c elfload.c)
target_include_directories(${EXECUTABLE_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
add_custom_target(system_bin ALL DEPENDS ${PROJECT_NAME}.bin)
add_custom_command(
   OUTPUT ${PROJECT_NAME}.bin
   DEPENDS ${EXECUTABLE_NAME}
   POST_BUILD
   COMMAND "${CMAKE_OBJCOPY}"
   ARGS ${EXECUTABLE_NAME} -O binary ${PROJECT_NAME}.bin
   )
set_target_properties(${TARGET_NAME} PROPERTIES LINK_DEPENDS ${LINKER_SCRIPT})

