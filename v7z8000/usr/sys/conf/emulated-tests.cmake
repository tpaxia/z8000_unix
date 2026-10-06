# Host test harness for the custom emulated machine.
add_subdirectory("${EMU_DIR}" "${B}/z8000_emu" EXCLUDE_FROM_ALL)
add_executable(test_driver "${DRIVER_DIR}/test_driver.cpp")
target_link_libraries(test_driver PRIVATE z8000)
target_include_directories(test_driver PRIVATE "${EMU_DIR}/src")

add_custom_target(disk_image
    COMMAND make -C "${TOOLS_DIR}" v7mkfs init
    COMMAND "${TOOLS_DIR}/v7mkfs" root.img proto.small
    COMMAND ${CMAKE_COMMAND} -E copy "${TOOLS_DIR}/root.img" "${B}/root.img"
    COMMAND ${CMAKE_COMMAND} -E copy "${TOOLS_DIR}/root.img" "${B}/hd.img"
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
            -x "libc: 37 passed, 0 failed"
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
    rom.o rom.lst rom.coff rom.bin
    trap.o trap.lst trap.coff kernel.bin
    krt.az8 krt.b arith.az8 arith.b
    handler.bout handler.bin
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
