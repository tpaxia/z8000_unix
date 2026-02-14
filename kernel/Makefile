# Kernel Test Makefile
# Builds ROM + kernel binaries and a test driver to verify
# SYSCALL trap handling on the Z8001.

CXX ?= c++
CXXFLAGS = -std=c++17 -Wall -Wextra

# Z8000 cross-toolchain
Z8K_PREFIX ?= z8k-coff-
Z8K_AS = $(Z8K_PREFIX)as
Z8K_LD = $(Z8K_PREFIX)ld
Z8K_OBJCOPY = $(Z8K_PREFIX)objcopy

# Emulator library
EMU_DIR = ../z8000_emu
EMU_LIB = $(EMU_DIR)/build/libz8000.a
EMU_INC = $(EMU_DIR)/include
EMU_TOOLS = $(EMU_DIR)/tools

.PHONY: all clean test emu_lib

all: rom.bin kernel.bin test_driver

# =============================================================================
# Ensure emulator library is built
# =============================================================================
emu_lib:
	$(MAKE) -C $(EMU_DIR) libz8000

$(EMU_LIB): emu_lib

# =============================================================================
# ROM binary (segment 0) - assembled in segmented mode
# =============================================================================
rom.o rom.lst: rom.s
	$(Z8K_AS) -z8001 -alns=rom.lst -o rom.o $<

rom.coff: rom.o
	$(Z8K_LD) -m z8001 -Ttext=0 -e 0 -o $@ $<

rom.bin: rom.coff
	$(Z8K_OBJCOPY) -O binary $< $@
	@echo "ROM: $$(wc -c < $@) bytes"

# =============================================================================
# Kernel binary (segment 1) - assembled in segmented mode
# (trap stubs use @RR14 for segmented stack pointer)
# The PSA table uses .word directives for segmented address constants.
# =============================================================================
trap.o trap.lst: trap.s
	$(Z8K_AS) -z8001 -alns=trap.lst -o trap.o $<

trap.coff: trap.o
	$(Z8K_LD) -m z8001 -Ttext=0 -e 0 -o $@ $<

kernel.bin: trap.coff
	$(Z8K_OBJCOPY) -O binary $< $@
	@echo "Kernel: $$(wc -c < $@) bytes"

# =============================================================================
# Test driver - links against emulator library
# =============================================================================
test_driver: test_driver.cpp $(EMU_LIB)
	$(CXX) $(CXXFLAGS) -I$(EMU_INC) -I$(EMU_TOOLS) -o $@ $< -L$(EMU_DIR)/build -lz8000

# =============================================================================
# Test target
# =============================================================================
test: all
	@echo ""
	@echo "=== Running kernel syscall test ==="
	@echo ""
	./test_driver -t

clean:
	rm -f *.o *.coff *.bin *.lst test_driver
