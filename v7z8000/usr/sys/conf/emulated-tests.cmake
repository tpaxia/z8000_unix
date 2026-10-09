# Host test harness for the custom emulated machine.
add_subdirectory("${EMU_DIR}" "${B}/z8000_emu" EXCLUDE_FROM_ALL)
add_executable(test_driver "${DRIVER_DIR}/test_driver.cpp")
target_link_libraries(test_driver PRIVATE z8000)
target_include_directories(test_driver PRIVATE "${EMU_DIR}/src")

add_custom_target(disk_image
    COMMAND python3 "${TOOLS_DIR}/native-cc/build.py" --image
    COMMAND ${CMAKE_COMMAND} -E copy "${TOOLS_DIR}/../tests/build/native-cc-sout/hd.img" "${B}/root.img"
    COMMAND ${CMAKE_COMMAND} -E copy "${TOOLS_DIR}/../tests/build/native-cc-sout/hd.img" "${B}/hd.img"
    WORKING_DIRECTORY "${TOOLS_DIR}"
    COMMENT "Building disk image"
)

# The Seventh Edition C library, exercised by tools/libctest.c under the kernel.
add_custom_target(libc_image
    COMMAND make -C "${TOOLS_DIR}" root-libc.img
    COMMAND ${CMAKE_COMMAND} -E copy "${TOOLS_DIR}/root-libc.img" "${B}/hd-libc.img"
    WORKING_DIRECTORY "${TOOLS_DIR}"
    COMMENT "Building C library test disk image"
)

add_custom_target(test-libc
    COMMAND ./test_driver -c 400000000 -d hd-libc.img -i "libctest\\nexit\\n"
            -x "libc: 39 passed, 0 failed"
    DEPENDS kernel test_driver disk_image libc_image
    WORKING_DIRECTORY "${B}"
    COMMENT "Running C library test under the kernel"
    VERBATIM
)

add_custom_target(test
    COMMAND ./test_driver -c 200000000
    DEPENDS kernel test_driver disk_image
    WORKING_DIRECTORY "${B}"
    COMMENT "Running kernel boot test"
)

add_custom_target(preempt_image
    COMMAND make -C "${TOOLS_DIR}" root-preempt.img
    COMMAND ${CMAKE_COMMAND} -E copy "${TOOLS_DIR}/root-preempt.img" "${B}/hd-preempt.img"
    WORKING_DIRECTORY "${TOOLS_DIR}"
    COMMENT "Building preemption test disk image"
)

add_custom_target(test-signal
    COMMAND ./test_driver -c 400000000 -d hd-preempt.img
            -i "signaltest\\n" -w "signal: complete" -I "exit\\n"
            -x "signal: all checks passed"
    COMMAND python3 "${TOOLS_DIR}/test-signal-shell.py" "${B}"
    DEPENDS kernel test_driver disk_image preempt_image
    WORKING_DIRECTORY "${B}"
    COMMENT "Testing caught signals and user context restoration"
    VERBATIM
)

add_custom_target(test-preempt
    COMMAND ./test_driver -c 400000000 -d hd-preempt.img
            -i "preempttest\\nexit\\n"
            -x "preempt: scheduling, signals and context passed"
    COMMAND ./test_driver -c 400000000 -d hd-preempt.img
            -i "preempttest console\\n" -w "preempt: waiting for input"
            -I "go\\nexit\\n" -x "preempt: console wakeup passed"
    DEPENDS kernel test_driver disk_image preempt_image
    WORKING_DIRECTORY "${B}"
    COMMENT "Testing CPU-bound preemption and signals"
    VERBATIM
)

# =============================================================================
# Clean
# =============================================================================
set_property(DIRECTORY PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
    rom.so rom.bin
    trap.so kernel.bin
    krt.az8 krt.b arith.az8 arith.b
    handler.sout handler.bin handler-data.bin
    root.img hd.img
)

add_custom_target(tty_image
    COMMAND make -C "${TOOLS_DIR}" root-tty.img
    COMMAND ${CMAKE_COMMAND} -E copy "${TOOLS_DIR}/root-tty.img" "${B}/hd-tty.img"
    WORKING_DIRECTORY "${TOOLS_DIR}"
    COMMENT "Building terminal test disk image"
)

add_custom_target(test-tty
    COMMAND python3 "${TOOLS_DIR}/test-tty.py" "${B}"
    DEPENDS kernel test_driver disk_image tty_image
    WORKING_DIRECTORY "${B}"
    COMMENT "Testing terminal settings, echo, cooked, cbreak and raw input"
    VERBATIM
)

add_custom_target(split_image
    COMMAND make -C "${TOOLS_DIR}" root-split.img
    COMMAND ${CMAKE_COMMAND} -E copy "${TOOLS_DIR}/root-split.img" "${B}/hd-split.img"
    WORKING_DIRECTORY "${TOOLS_DIR}"
    COMMENT "Building separate instruction/data test disk"
)

add_custom_target(test-split
    COMMAND python3 "${TOOLS_DIR}/test-split.py" "${B}"
    DEPENDS kernel test_driver disk_image split_image
    WORKING_DIRECTORY "${B}"
    COMMENT "Testing split I/D loading, data, fork, exec, libc and signals"
    VERBATIM
)

add_custom_target(test-fpe
    COMMAND python3 ${TOOLS_DIR}/test-fpe.py ${B}
    DEPENDS kernel test_driver disk_image
    WORKING_DIRECTORY ${B}
    COMMENT "Testing separate software EPU, arithmetic and process state"
    VERBATIM)

add_custom_target(test-copy
    COMMAND python3 "${TOOLS_DIR}/test-copy.py" "${B}"
    DEPENDS kernel test_driver disk_image
    WORKING_DIRECTORY "${B}"
    COMMENT "Testing V7 copy dispatch and fault accounting on the target ABI"
    VERBATIM
)

add_custom_target(test-fault
    COMMAND python3 "${TOOLS_DIR}/test-fault.py" "${B}"
    DEPENDS kernel test_driver disk_image
    WORKING_DIRECTORY "${B}"
    COMMENT "Testing user address bounds and SEGTRAP recovery"
    VERBATIM
)

add_custom_target(test-v7-interfaces
    COMMAND python3 "${TOOLS_DIR}/test-v7-interfaces.py" "${B}"
    DEPENDS kernel test_driver disk_image
    WORKING_DIRECTORY "${B}"
    COMMENT "Testing restored V7 filesystem and TTY interfaces"
    VERBATIM
)

add_custom_target(test-bio
    COMMAND python3 "${TOOLS_DIR}/test-bio.py" "${B}"
    DEPENDS kernel test_driver disk_image
    WORKING_DIRECTORY "${B}"
    COMMENT "Testing buffer-cache policy, deferred HD completions and reboot persistence"
    VERBATIM
)

add_custom_target(test-abi
    COMMAND python3 "${TOOLS_DIR}/test-abi.py" "${B}"
    DEPENDS kernel test_driver disk_image
    WORKING_DIRECTORY "${B}"
    COMMENT "Testing V7 syscall numbers and exec environments in both layouts"
    VERBATIM
)

add_executable(memory_test "${DRIVER_DIR}/memory_test.cpp")
target_link_libraries(memory_test PRIVATE z8000)
target_include_directories(memory_test PRIVATE "${EMU_DIR}/src")
add_custom_target(test-memory
    COMMAND ./memory_test
    COMMAND python3 "${TOOLS_DIR}/test-memory.py" "${B}"
    DEPENDS kernel test_driver disk_image memory_test
    WORKING_DIRECTORY "${B}"
    COMMENT "Testing resource maps, physical RAM bounds and low-memory fork recovery"
    VERBATIM
)

add_custom_target(test-physio
    COMMAND python3 "${TOOLS_DIR}/test-physio.py" "${B}"
    DEPENDS kernel test_driver disk_image
    WORKING_DIRECTORY "${B}"
    COMMENT "Testing raw disk transfers, mapping ownership and low-memory swapping"
    VERBATIM
)

add_custom_target(test-core
    COMMAND python3 "${TOOLS_DIR}/test-core.py" "${B}"
    DEPENDS kernel test_driver disk_image
    WORKING_DIRECTORY "${B}"
    COMMENT "Testing V7 core images, registers, permissions, disk exhaustion and swapping"
    VERBATIM
)

add_custom_target(test-ptrace
    COMMAND python3 "${TOOLS_DIR}/test-ptrace.py" "${B}"
    DEPENDS kernel test_driver disk_image
    WORKING_DIRECTORY "${B}"
    COMMENT "Testing V7 tracing, register writes, protected text and stopped-process swapping"
    VERBATIM
)

add_custom_target(test-exec
    COMMAND python3 "${TOOLS_DIR}/test-exec.py" "${B}"
    DEPENDS kernel test_driver disk_image
    WORKING_DIRECTORY "${B}"
    COMMENT "Testing set-ID exec, CPU startup, tracing and credential core policy"
    VERBATIM
)

add_custom_target(test-services
    COMMAND python3 "${TOOLS_DIR}/test-services.py" "${B}"
    DEPENDS kernel test_driver disk_image
    WORKING_DIRECTORY "${B}"
    COMMENT "Testing public ABI, accounting, profiling and memory locking"
    VERBATIM
)
