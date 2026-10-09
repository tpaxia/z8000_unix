cmake_minimum_required(VERSION 3.16)
project(z8000_unix_kernel LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Shorthand for build directory (used in OUTPUT/DEPENDS for correct resolution)
set(B "${CMAKE_CURRENT_BINARY_DIR}")

# --- Configurable toolchain paths ---

set(PCC_ROOT "${CMAKE_SOURCE_DIR}/../../../PCC-z8000/z8000" CACHE PATH "PCC toolchain root")
set(CZ8 "${PCC_ROOT}/cz8/cz8")
set(PORT_TOOLS "${CMAKE_SOURCE_DIR}/../../../tools")
set(HOST_CC "cc" CACHE STRING "Host C compiler for shared assembler/linker")
set(ASZ8K "${B}/asz8k")
set(LDZ8 "${B}/ldz8")
file(GLOB ASZ_SOURCES CONFIGURE_DEPENDS "${PORT_TOOLS}/asz8k/src/*.c")
file(GLOB ASZ_HEADERS CONFIGURE_DEPENDS "${PORT_TOOLS}/asz8k/src/*.h")
add_custom_command(OUTPUT "${ASZ8K}"
    COMMAND ${HOST_CC} -std=gnu89 -O2 -fwrapv -fsigned-char -fcommon -w
        -Wno-int-conversion -Wno-return-mismatch -DASZ_HOST ${ASZ_SOURCES} -o "${ASZ8K}"
    DEPENDS ${ASZ_SOURCES} ${ASZ_HEADERS}
    COMMENT "Building host copy of native assembler")
configure_file("${PORT_TOOLS}/asz8k/src/asz8k.pd" "${B}/asz8k.pd" COPYONLY)
add_custom_command(OUTPUT "${LDZ8}"
    COMMAND ${HOST_CC} -std=gnu89 -O2 -fwrapv -fcommon -w
        -Wno-int-conversion -Wno-return-mismatch -I${PCC_ROOT} -I${PORT_TOOLS}/asz8k/src
        ${PORT_TOOLS}/ldz8/dispatch.c ${PORT_TOOLS}/ldz8/ldso.c
        ${PORT_TOOLS}/asz8k/src/soutfmt.c -o "${LDZ8}"
    DEPENDS ${PORT_TOOLS}/ldz8/dispatch.c ${PORT_TOOLS}/ldz8/ldso.c
        ${PORT_TOOLS}/asz8k/src/soutfmt.c ${PORT_TOOLS}/asz8k/src/soutfmt.h
    COMMENT "Building host copy of native linker")
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
add_custom_command(OUTPUT ${B}/rom.so
    COMMAND ${ASZ8K} -zgs -o rom.so ${CMAKE_SOURCE_DIR}/${KERNEL_ROM}
    DEPENDS ${CMAKE_SOURCE_DIR}/${KERNEL_ROM} "${CONFIG_STAMP}" "${ASZ8K}" ${B}/asz8k.pd
    COMMENT "Assembling reset ROM with shared assembler")
add_custom_command(OUTPUT ${B}/rom.bin
    COMMAND ${LDZ8} -z -b -C 0 rom.so -o rom.bin
    DEPENDS ${B}/rom.so "${LDZ8}"
    COMMENT "Linking reset ROM image")

# SEG/NONSEG trap stubs occupy the first 512 kernel instruction bytes.
add_custom_command(OUTPUT ${B}/trap.so
    COMMAND ${ASZ8K} -zgs -o trap.so ${CMAKE_SOURCE_DIR}/${KERNEL_TRAPS}
    DEPENDS ${CMAKE_SOURCE_DIR}/${KERNEL_TRAPS} "${CONFIG_STAMP}" "${ASZ8K}" ${B}/asz8k.pd
    COMMENT "Assembling trap stubs with shared assembler")
add_custom_command(OUTPUT ${B}/kernel.bin
    COMMAND ${LDZ8} -z -b -C 1 -M 512 trap.so -o kernel.bin
    DEPENDS ${B}/trap.so "${LDZ8}"
    COMMENT "Linking trap image")

# =============================================================================
# PCC compilation function
# =============================================================================

# pcc_compile_c(<subdir> <name>)
# Compiles <subdir>/<name>.c → <subdir>/<name>.az8 → <subdir>/<name>.so
# Appends the .so path to HANDLER_B_FILES in parent scope.
# COMMAND arguments use relative paths while OUTPUT/DEPENDS use absolute paths.
# Every kernel source may include any kernel header.
file(GLOB KERNEL_HEADERS "${CMAKE_SOURCE_DIR}/h/*.h")

function(pcc_compile_c subdir name)
    set(SRC "${CMAKE_SOURCE_DIR}/${subdir}/${name}.c")
    set(AZ8_REL "${subdir}/${name}.az8")
    set(B_REL "${subdir}/${name}.so")

    add_custom_command(OUTPUT "${B}/${AZ8_REL}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${subdir}"
        COMMAND sh "${PCC_COMPILE_SH}" "${CMAKE_SOURCE_DIR}/h" "${SRC}" "${CZ8}" "${AZ8_REL}" "${OZ8}"
        DEPENDS "${SRC}" "${CONFIG_STAMP}" ${KERNEL_HEADERS} "${CZ8}" "${PCC_COMPILE_SH}" "${OZ8}"
        COMMENT "PCC: ${subdir}/${name}.c → .az8"
    )
    add_custom_command(OUTPUT "${B}/${B_REL}"
        COMMAND ${ASZ8K} -zc -o "${B_REL}" "${AZ8_REL}"
        DEPENDS "${B}/${AZ8_REL}" "${ASZ8K}"
        COMMENT "PCC: ${subdir}/${name}.az8 → .so"
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
    add_custom_command(OUTPUT "${B}/${output}.so"
        COMMAND ${ASZ8K} -zc -o "${output}.so" "${output}.az8"
        DEPENDS "${B}/${output}.az8" "${ASZ8K}")
    list(APPEND HANDLER_B_FILES "${output}.so")
endforeach()

# Shared V7 services. Machine configuration never changes this list.
foreach(name main acct bio physio malloc text alloc iget nami rdwri subr fio prf slp
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
add_custom_command(OUTPUT ${B}/arith.so
    COMMAND ${ASZ8K} -zc -o arith.so arith.az8
    DEPENDS ${B}/arith.az8 "${ASZ8K}"
    COMMENT "PCC: arith.az8 → arith.so"
)
list(APPEND HANDLER_B_FILES arith.so)
add_custom_command(OUTPUT ${B}/csv.so
    COMMAND ${CMAKE_COMMAND} -E copy ${PCC_ROOT}/lib/csv.az8 csv.az8
    COMMAND ${ASZ8K} -zc -o csv.so csv.az8
    DEPENDS ${PCC_ROOT}/lib/csv.az8 ${ASZ8K})
list(APPEND HANDLER_B_FILES csv.so)

# Link all .so files → handler.sout → handler.bin
# Convert relative .so paths to absolute for DEPENDS
set(HANDLER_B_ABS "")
foreach(f ${HANDLER_B_FILES})
    list(APPEND HANDLER_B_ABS "${B}/${f}")
endforeach()

add_custom_command(OUTPUT ${B}/handler.sout
    COMMAND ${LDZ8} -z -i -x ${HANDLER_B_FILES} -o handler.sout
    DEPENDS ${HANDLER_B_ABS} "${LDZ8}" "${CONFIG_STAMP}"
    COMMENT "Linking handler.sout"
)
add_custom_command(OUTPUT ${B}/handler.bin ${B}/handler-data.bin
    COMMAND python3 "${TOOLS_DIR}/sout2bin.py" handler.sout handler.bin handler-data.bin
    DEPENDS ${B}/handler.sout "${TOOLS_DIR}/sout2bin.py"
    COMMENT "Building handler.bin"
)

# Kernel artifacts are independent of the host emulator/test harness.
add_custom_target(kernel ALL DEPENDS
    ${B}/rom.bin ${B}/kernel.bin ${B}/handler.bin ${B}/handler-data.bin ${B}/fpe.bin)
add_custom_target(check_headers
    COMMAND python3 ${TOOLS_DIR}/export-headers.py --check
    COMMENT "Checking installed kernel ABI headers")
add_dependencies(kernel check_headers)

add_custom_command(OUTPUT ${B}/fpe.bin
    COMMAND ${CMAKE_COMMAND} -E make_directory ${B}/fpe
    COMMAND ${CMAKE_COMMAND} -E copy ${B}/asz8k.pd ${B}/fpe/asz8k.pd
    COMMAND python3 ${TOOLS_DIR}/fpe/translate.py ${CMAKE_SOURCE_DIR}/fpe/fpe.z8k ${B}/fpe/core.s
    COMMAND ${ASZ8K} -zgs -o core.so core.s
    COMMAND ${ASZ8K} -zgs -o unix.so ${CMAKE_SOURCE_DIR}/fpe/unix.s
    COMMAND ${LDZ8} -z -b -C 127 -M 61440 unix.so core.so -o ${B}/fpe.bin
    WORKING_DIRECTORY ${B}/fpe
    DEPENDS ${CMAKE_SOURCE_DIR}/fpe/fpe.z8k ${CMAKE_SOURCE_DIR}/fpe/unix.s
            ${TOOLS_DIR}/fpe/translate.py "${ASZ8K}" "${LDZ8}" ${B}/asz8k.pd
    COMMENT "Building separate Zilog software EPU service")
file(MAKE_DIRECTORY ${B}/fpe)

# A board can provide its own image builder and tests, or none.
option(KERNEL_HOST_TESTS "Build the selected machine host harness and test targets" ON)
if(KERNEL_HOST_TESTS AND KERNEL_TEST_FILE)
    include("${CMAKE_SOURCE_DIR}/${KERNEL_TEST_FILE}")
endif()
