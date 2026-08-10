cmake_minimum_required(VERSION 3.16)
project(z8000_unix_kernel LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Shorthand for build directory (used in OUTPUT/DEPENDS for correct resolution)
set(B "${CMAKE_CURRENT_BINARY_DIR}")

# --- Configurable toolchain paths ---
set(Z8K_PREFIX "z8k-coff-" CACHE STRING "Z8K cross-toolchain prefix")
set(Z8K_AS "${Z8K_PREFIX}as")
set(Z8K_LD "${Z8K_PREFIX}ld")
set(Z8K_OBJCOPY "${Z8K_PREFIX}objcopy")

set(PCC_ROOT "${CMAKE_SOURCE_DIR}/../../../PCC-z8000/z8000" CACHE PATH "PCC toolchain root")
set(CZ8 "${PCC_ROOT}/cz8/cz8")
set(AZ8 "${PCC_ROOT}/az8/az8")
set(LDZ8 "${PCC_ROOT}/ldz8")

set(EMU_DIR "${CMAKE_SOURCE_DIR}/../../../z8000_emu" CACHE PATH "z8000_emu directory")
set(TOOLS_DIR "${CMAKE_SOURCE_DIR}/../../../tools" CACHE PATH "Tools directory")
set(DRIVER_DIR "${CMAKE_SOURCE_DIR}/../../../emu" CACHE PATH "Emulator front end directory")

# --- Generate PCC compile helper script (avoids shell quoting issues) ---
set(PCC_COMPILE_SH "${B}/pcc_compile.sh")
file(WRITE "${PCC_COMPILE_SH}" [=[#!/bin/sh
# Usage: pcc_compile.sh <include_dir> <source.c> <cz8_path> <output.az8>
cpp -Dz8000 -Dz8002 -I"$1" "$2" | "$3" 2>&1 | grep -v '^"' > "$4"
]=])

# =============================================================================
# ROM binary (segment 0) — Z8K segmented assembly
# =============================================================================
add_custom_command(OUTPUT ${B}/rom.o ${B}/rom.lst
    COMMAND ${Z8K_AS} -z8001 -alns=rom.lst -o rom.o ${CMAKE_SOURCE_DIR}/rom.s
    DEPENDS ${CMAKE_SOURCE_DIR}/rom.s
    COMMENT "Assembling rom.s"
)
add_custom_command(OUTPUT ${B}/rom.coff
    COMMAND ${Z8K_LD} -m z8001 -Ttext=0 -e 0 -o rom.coff rom.o
    DEPENDS ${B}/rom.o
    COMMENT "Linking rom.coff"
)
add_custom_command(OUTPUT ${B}/rom.bin
    COMMAND ${Z8K_OBJCOPY} -O binary rom.coff rom.bin
    DEPENDS ${B}/rom.coff
    COMMENT "Building rom.bin"
)

# =============================================================================
# Kernel binary (segment 1) — Z8K segmented assembly
# =============================================================================
add_custom_command(OUTPUT ${B}/trap.o ${B}/trap.lst
    COMMAND ${Z8K_AS} -z8001 -alns=trap.lst -o trap.o ${CMAKE_SOURCE_DIR}/trap.s
    DEPENDS ${CMAKE_SOURCE_DIR}/trap.s
    COMMENT "Assembling trap.s"
)
add_custom_command(OUTPUT ${B}/trap.coff
    COMMAND ${Z8K_LD} -m z8001 -Ttext=0 -e 0 -o trap.coff trap.o
    DEPENDS ${B}/trap.o
    COMMENT "Linking trap.coff"
)
add_custom_command(OUTPUT ${B}/kernel.bin
    COMMAND ${Z8K_OBJCOPY} -O binary trap.coff kernel.bin
    DEPENDS ${B}/trap.coff
    COMMENT "Building kernel.bin"
)

# =============================================================================
# PCC compilation function
# =============================================================================

# pcc_compile_c(<subdir> <name>)
# Compiles <subdir>/<name>.c → <subdir>/<name>.az8 → <subdir>/<name>.b
# Appends the .b path to HANDLER_B_FILES in parent scope.
# NOTE: az8 has a 32-byte filename buffer (STR_MAX), so COMMAND arguments
# use relative paths while OUTPUT/DEPENDS use absolute paths for CMake.
function(pcc_compile_c subdir name)
    set(SRC "${CMAKE_SOURCE_DIR}/${subdir}/${name}.c")
    set(AZ8_REL "${subdir}/${name}.az8")
    set(B_REL "${subdir}/${name}.b")

    add_custom_command(OUTPUT "${B}/${AZ8_REL}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${subdir}"
        COMMAND sh "${PCC_COMPILE_SH}" "${CMAKE_SOURCE_DIR}/h" "${SRC}" "${CZ8}" "${AZ8_REL}"
        DEPENDS "${SRC}"
        COMMENT "PCC: ${subdir}/${name}.c → .az8"
    )
    add_custom_command(OUTPUT "${B}/${B_REL}"
        COMMAND ${AZ8} -o "${B_REL}" "${AZ8_REL}"
        DEPENDS "${B}/${AZ8_REL}"
        COMMENT "PCC: ${subdir}/${name}.az8 → .b"
    )

    set(HANDLER_B_FILES ${HANDLER_B_FILES} "${B_REL}" PARENT_SCOPE)
endfunction()

# =============================================================================
# Handler binary — PCC pipeline
# =============================================================================

# krt.s: copy to .az8 then assemble
add_custom_command(OUTPUT ${B}/krt.az8
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/krt.s krt.az8
    DEPENDS ${CMAKE_SOURCE_DIR}/krt.s
    COMMENT "PCC: krt.s → krt.az8"
)
add_custom_command(OUTPUT ${B}/krt.b
    COMMAND ${AZ8} -o krt.b krt.az8
    DEPENDS ${B}/krt.az8
    COMMENT "PCC: krt.az8 → krt.b"
)

set(HANDLER_B_FILES krt.b)

# Kernel C sources (sys/)
pcc_compile_c(sys main)
pcc_compile_c(sys bio)
pcc_compile_c(sys alloc)
pcc_compile_c(sys iget)
pcc_compile_c(sys nami)
pcc_compile_c(sys rdwri)
pcc_compile_c(sys subr)
pcc_compile_c(sys fio)
pcc_compile_c(sys prf)
pcc_compile_c(sys machdep)
pcc_compile_c(sys slp)
pcc_compile_c(sys trap)
pcc_compile_c(sys sys1)
pcc_compile_c(sys sys2)
pcc_compile_c(sys sys3)
pcc_compile_c(sys sys4)
pcc_compile_c(sys pipe)
pcc_compile_c(sys sig)
pcc_compile_c(sys sysent)
pcc_compile_c(sys clock)
pcc_compile_c(sys prim)

# Device driver C sources (dev/)
pcc_compile_c(dev md)
pcc_compile_c(dev hd)
pcc_compile_c(dev cons)
pcc_compile_c(dev conf)
pcc_compile_c(dev tty)
pcc_compile_c(dev partab)

# Arithmetic library
add_custom_command(OUTPUT ${B}/arith.az8
    COMMAND ${CMAKE_COMMAND} -E copy "${PCC_ROOT}/lib/arith.az8" arith.az8
    DEPENDS "${PCC_ROOT}/lib/arith.az8"
    COMMENT "PCC: copying arith.az8"
)
add_custom_command(OUTPUT ${B}/arith.b
    COMMAND ${AZ8} -o arith.b arith.az8
    DEPENDS ${B}/arith.az8
    COMMENT "PCC: arith.az8 → arith.b"
)
list(APPEND HANDLER_B_FILES arith.b)

# Link all .b files → handler.bout → handler.bin
# Convert relative .b paths to absolute for DEPENDS
set(HANDLER_B_ABS "")
foreach(f ${HANDLER_B_FILES})
    list(APPEND HANDLER_B_ABS "${B}/${f}")
endforeach()

add_custom_command(OUTPUT ${B}/handler.bout
    COMMAND ${LDZ8} -x -R 200 ${HANDLER_B_FILES} -o handler.bout
    DEPENDS ${HANDLER_B_ABS}
    COMMENT "Linking handler.bout"
)
add_custom_command(OUTPUT ${B}/handler.bin
    COMMAND python3 "${TOOLS_DIR}/bout2bin.py" handler.bout handler.bin
    DEPENDS ${B}/handler.bout "${TOOLS_DIR}/bout2bin.py"
    COMMENT "Building handler.bin"
)

# =============================================================================
# Emulator library (external CMake project)
# =============================================================================
add_subdirectory("${EMU_DIR}" "${B}/z8000_emu" EXCLUDE_FROM_ALL)

# =============================================================================
# Test driver — native C++ linked against emulator
# =============================================================================
add_executable(test_driver "${DRIVER_DIR}/test_driver.cpp")
target_link_libraries(test_driver PRIVATE z8000)
target_include_directories(test_driver PRIVATE "${EMU_DIR}/src")

# =============================================================================
# Top-level targets
# =============================================================================
add_custom_target(kernel ALL DEPENDS
    ${B}/rom.bin ${B}/kernel.bin ${B}/handler.bin test_driver)

add_custom_target(disk_image
    COMMAND make -C "${TOOLS_DIR}" v7mkfs init
    COMMAND "${TOOLS_DIR}/v7mkfs" root.img proto.small
    COMMAND ${CMAKE_COMMAND} -E copy "${TOOLS_DIR}/root.img" "${B}/root.img"
    COMMAND ${CMAKE_COMMAND} -E copy "${TOOLS_DIR}/root.img" "${B}/hd.img"
    WORKING_DIRECTORY "${TOOLS_DIR}"
    COMMENT "Building disk image"
)

add_custom_target(test
    COMMAND ./test_driver -c 200000000
    DEPENDS kernel disk_image
    WORKING_DIRECTORY "${B}"
    COMMENT "Running kernel boot test"
)

# =============================================================================
# Clean
# =============================================================================
set_property(DIRECTORY PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
    rom.o rom.lst rom.coff rom.bin
    trap.o trap.lst trap.coff kernel.bin
    krt.az8 krt.b arith.az8 arith.b
    handler.bout handler.bin
    root.img hd.img
)
