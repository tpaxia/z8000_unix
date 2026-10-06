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
set(OZ8 "${B}/oz8")
add_custom_command(OUTPUT "${OZ8}"
    COMMAND cc -std=gnu89 -O -w -Wno-return-mismatch -Wno-int-conversion
            -o "${OZ8}" "${PCC_ROOT}/oz8.c"
    DEPENDS "${PCC_ROOT}/oz8.c"
    COMMENT "Building host copy of native assembly optimizer")

set(EMU_DIR "${CMAKE_SOURCE_DIR}/../../../z8000_emu" CACHE PATH "z8000_emu directory")
set(TOOLS_DIR "${CMAKE_SOURCE_DIR}/../../../tools" CACHE PATH "Tools directory")
set(DRIVER_DIR "${CMAKE_SOURCE_DIR}/../../../emu" CACHE PATH "Emulator front end directory")

# A configuration selects drivers, machine/MMU support and interrupt wiring.
set(KERNEL_CONFIG "emulated" CACHE STRING "Kernel configuration from conf/<name>.cmake")
if(NOT KERNEL_CONFIG MATCHES "^[A-Za-z0-9_-]+$")
    message(FATAL_ERROR "Invalid KERNEL_CONFIG: ${KERNEL_CONFIG}")
endif()
set(CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/conf/${KERNEL_CONFIG}.cmake")
if(NOT EXISTS "${CONFIG_FILE}")
    message(FATAL_ERROR "Unknown kernel configuration: ${KERNEL_CONFIG}")
endif()
include("${CONFIG_FILE}")
if(NOT DEFINED KERNEL_OPTIONAL_C)
    set(KERNEL_OPTIONAL_C sys/fakemx.c)
endif()
foreach(required KERNEL_ROM KERNEL_TRAPS KERNEL_ASM KERNEL_MACHINE_C KERNEL_DRIVERS)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${CONFIG_FILE} must define ${required}")
    endif()
endforeach()
# Re-selecting a configuration must invalidate outputs even if sources are older.
set(CONFIG_STAMP "${B}/kernel-selection.txt")
file(GENERATE OUTPUT "${CONFIG_STAMP}"
    CONTENT "${KERNEL_CONFIG}\n${KERNEL_ROM}\n${KERNEL_TRAPS}\n${KERNEL_ASM}\n${KERNEL_MACHINE_C}\n${KERNEL_DRIVERS}\n${KERNEL_OPTIONAL_C}\n")
message(STATUS "Kernel configuration: ${KERNEL_CONFIG}")

# --- Generate PCC compile helper script (avoids shell quoting issues) ---
set(PCC_COMPILE_SH "${B}/pcc_compile.sh")
file(WRITE "${PCC_COMPILE_SH}" [=[#!/bin/sh
# Usage: pcc_compile.sh <include_dir> <source.c> <cz8_path> <output.az8> <oz8_path>
# cz8 writes assembly to stdout and diagnostics to stderr. Diagnostics are
# left on stderr so warnings are seen, and any preprocessor or compiler
# failure fails the build instead of leaving truncated assembly behind.
cpp -Dz8000 -Dz8002 -I"$1" "$2" > "$4.i" || { rm -f "$4.i" "$4"; exit 1; }
"$3" < "$4.i" > "$4.raw"
status=$?
rm -f "$4.i"
if [ $status -ne 0 ]; then
	rm -f "$4.raw" "$4"
	echo "cz8 failed on $2" >&2
	exit 1
fi
"$5" < "$4.raw" > "$4" || exit 1
rm -f "$4.raw"
]=])

# =============================================================================
# ROM binary (segment 0) — Z8K segmented assembly
# =============================================================================
add_custom_command(OUTPUT ${B}/rom.o ${B}/rom.lst
    COMMAND ${Z8K_AS} -z8001 -alns=rom.lst -o rom.o ${CMAKE_SOURCE_DIR}/${KERNEL_ROM}
    DEPENDS ${CMAKE_SOURCE_DIR}/${KERNEL_ROM} "${CONFIG_STAMP}"
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
    COMMAND ${Z8K_AS} -z8001 -alns=trap.lst -o trap.o ${CMAKE_SOURCE_DIR}/${KERNEL_TRAPS}
    DEPENDS ${CMAKE_SOURCE_DIR}/${KERNEL_TRAPS} "${CONFIG_STAMP}"
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
# Every kernel source may include any kernel header.
file(GLOB KERNEL_HEADERS "${CMAKE_SOURCE_DIR}/h/*.h")

function(pcc_compile_c subdir name)
    set(SRC "${CMAKE_SOURCE_DIR}/${subdir}/${name}.c")
    set(AZ8_REL "${subdir}/${name}.az8")
    set(B_REL "${subdir}/${name}.b")

    add_custom_command(OUTPUT "${B}/${AZ8_REL}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${subdir}"
        COMMAND sh "${PCC_COMPILE_SH}" "${CMAKE_SOURCE_DIR}/h" "${SRC}" "${CZ8}" "${AZ8_REL}" "${OZ8}"
        DEPENDS "${SRC}" "${CONFIG_STAMP}" ${KERNEL_HEADERS} "${CZ8}" "${PCC_COMPILE_SH}" "${OZ8}"
        COMMENT "PCC: ${subdir}/${name}.c → .az8"
    )
    add_custom_command(OUTPUT "${B}/${B_REL}"
        COMMAND ${AZ8} -o "${B_REL}" "${AZ8_REL}"
        DEPENDS "${B}/${AZ8_REL}" "${AZ8}"
        COMMENT "PCC: ${subdir}/${name}.az8 → .b"
    )

    set(HANDLER_B_FILES ${HANDLER_B_FILES} "${B_REL}" PARENT_SCOPE)
endfunction()

# =============================================================================
# Handler binary — PCC pipeline
# =============================================================================

# The runtime entry object must be first: trap entry calls its fixed jump table.
set(HANDLER_B_FILES "")
foreach(source IN LISTS KERNEL_ASM)
    get_filename_component(name "${source}" NAME_WE)
    set(output "machine/${name}")
    add_custom_command(OUTPUT "${B}/${output}.az8"
        COMMAND ${CMAKE_COMMAND} -E make_directory machine
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/${source}" "${output}.az8"
        DEPENDS "${CMAKE_SOURCE_DIR}/${source}" "${CONFIG_STAMP}")
    add_custom_command(OUTPUT "${B}/${output}.b"
        COMMAND ${AZ8} -o "${output}.b" "${output}.az8"
        DEPENDS "${B}/${output}.az8" "${AZ8}")
    list(APPEND HANDLER_B_FILES "${output}.b")
endforeach()

# Shared V7 services. Machine configuration never changes this list.
foreach(name main bio malloc text alloc iget nami rdwri subr fio prf slp
             sys1 sys2 sys3 sys4 pipe sig sysent clock prim)
    pcc_compile_c(sys ${name})
endforeach()
foreach(name tty partab)
    pcc_compile_c(dev ${name})
endforeach()
foreach(source IN LISTS KERNEL_MACHINE_C KERNEL_OPTIONAL_C)
    get_filename_component(directory "${source}" DIRECTORY)
    get_filename_component(name "${source}" NAME_WE)
    pcc_compile_c("${directory}" "${name}")
endforeach()
foreach(name IN LISTS KERNEL_DRIVERS)
    pcc_compile_c(dev ${name})
endforeach()

# Arithmetic library
add_custom_command(OUTPUT ${B}/arith.az8
    COMMAND ${CMAKE_COMMAND} -E copy "${PCC_ROOT}/lib/arith.az8" arith.az8
    DEPENDS "${PCC_ROOT}/lib/arith.az8"
    COMMENT "PCC: copying arith.az8"
)
add_custom_command(OUTPUT ${B}/arith.b
    COMMAND ${AZ8} -o arith.b arith.az8
    DEPENDS ${B}/arith.az8 "${AZ8}"
    COMMENT "PCC: arith.az8 → arith.b"
)
list(APPEND HANDLER_B_FILES arith.b)
add_custom_command(OUTPUT ${B}/csv.b
    COMMAND ${CMAKE_COMMAND} -E copy ${PCC_ROOT}/lib/csv.az8 csv.az8
    COMMAND ${AZ8} -o csv.b csv.az8
    DEPENDS ${PCC_ROOT}/lib/csv.az8 ${AZ8})
list(APPEND HANDLER_B_FILES csv.b)

# Link all .b files → handler.bout → handler.bin
# Convert relative .b paths to absolute for DEPENDS
set(HANDLER_B_ABS "")
foreach(f ${HANDLER_B_FILES})
    list(APPEND HANDLER_B_ABS "${B}/${f}")
endforeach()

add_custom_command(OUTPUT ${B}/handler.bout
    COMMAND ${LDZ8} -i -x ${HANDLER_B_FILES} -o handler.bout
    DEPENDS ${HANDLER_B_ABS} "${LDZ8}" "${CONFIG_STAMP}"
    COMMENT "Linking handler.bout"
)
add_custom_command(OUTPUT ${B}/handler.bin ${B}/handler-data.bin
    COMMAND python3 "${TOOLS_DIR}/bout2bin.py" handler.bout handler.bin handler-data.bin
    DEPENDS ${B}/handler.bout "${TOOLS_DIR}/bout2bin.py"
    COMMENT "Building handler.bin"
)

# Kernel artifacts are independent of the host emulator/test harness.
add_custom_target(kernel ALL DEPENDS
    ${B}/rom.bin ${B}/kernel.bin ${B}/handler.bin ${B}/handler-data.bin ${B}/fpe.bin)

add_custom_command(OUTPUT ${B}/fpe.bin
    COMMAND ${CMAKE_COMMAND} -E make_directory ${B}/fpe
    COMMAND python3 ${TOOLS_DIR}/fpe/translate.py ${CMAKE_SOURCE_DIR}/fpe/fpe.z8k ${B}/fpe/core.s
    COMMAND ${Z8K_AS} -z8002 -o core.o core.s
    COMMAND ${Z8K_AS} -z8001 -o unix.o ${CMAKE_SOURCE_DIR}/fpe/unix.s
    COMMAND ${Z8K_LD} -m z8002 -T ${CMAKE_SOURCE_DIR}/fpe/service.ld -o fpe.coff unix.o core.o
    COMMAND ${Z8K_OBJCOPY} -O binary fpe.coff ${B}/fpe.bin
    WORKING_DIRECTORY ${B}/fpe
    DEPENDS ${CMAKE_SOURCE_DIR}/fpe/fpe.z8k ${CMAKE_SOURCE_DIR}/fpe/unix.s
            ${CMAKE_SOURCE_DIR}/fpe/service.ld ${TOOLS_DIR}/fpe/translate.py
    COMMENT "Building separate Zilog software EPU service")
file(MAKE_DIRECTORY ${B}/fpe)

# A board can provide its own image builder and tests, or none.
option(KERNEL_HOST_TESTS "Build the selected machine host harness and test targets" ON)
if(KERNEL_HOST_TESTS AND KERNEL_TEST_FILE)
    include("${CMAKE_SOURCE_DIR}/${KERNEL_TEST_FILE}")
endif()
