cmake_minimum_required(VERSION 3.18.1)

set(LINKER_SCRIPT    "system.ld")
set(CMAKE_ASM_FLAGS  "-march=${ARCH}")
set(CMAKE_C_FLAGS    "-march=${ARCH} -mabi=${ABI} -DSYSTEM -O2")
set(CMAKE_EXE_LINKER_FLAGS "-T ${CMAKE_CURRENT_SOURCE_DIR}/${LINKER_SCRIPT}")
set(CMAKE_C_LINK_EXECUTABLE "<CMAKE_LINKER> <LINK_FLAGS> -o <TARGET> <OBJECTS>")

project(filestick-system)
set(EXECUTABLE_NAME "${PROJECT_NAME}.elf")

#set_property(SOURCE dev_open.c console.c strncmp.c strcmp.c rgbled.c brk.c exit.c spi_flashdev.c elfload.c strlen.c elfload.c memcpy.c PROPERTY COMPILE_FLAGS -O2)

enable_language(C ASM)
include_directories(BEFORE ../include)
add_executable(${EXECUTABLE_NAME} init.S super_trap.s isr_trap.S timer.s serial_putc.S spi_flash.S econet_rx.S get_csr.S fd.c dev_open.c memset.S memcpy.c console.c raw_econet.c strncmp.c strcmp.c strlcpy.c strtok.c rgbled.c brk.c exit.c spi_flashdev.c elfload.c strlen.c elfload.c crash.c regdump.c debug_syscall.c spi.S sd_intr.S sd_io.c sd_ldio.c diskio.c ff.c ffunicode.c mount.c directory.c memcmp.c strchr.c file.c file_ops.c printk.c super_shell.c hexdump.c flashdisc.c tlsf.c kmalloc.c)
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

