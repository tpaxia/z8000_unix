# Kernel Makefile
# Builds ROM + kernel binaries and a test driver that boots
# the V7 kernel to process 0 with filesystem and console.
#
# All build artifacts go into $(B)/ to keep the source tree clean.

CXX ?= c++
CXXFLAGS = -std=c++17 -Wall -Wextra

# Build directory
B = build

# Z8000 cross-toolchain (segmented mode, for rom.s and trap.s only)
Z8K_PREFIX ?= z8k-coff-
Z8K_AS = $(Z8K_PREFIX)as
Z8K_LD = $(Z8K_PREFIX)ld
Z8K_OBJCOPY = $(Z8K_PREFIX)objcopy

# PCC toolchain (non-segmented mode, for kernel C and krt.s)
PCC_ROOT = ../../../PCC-z8000/z8000
CZ8      = $(PCC_ROOT)/cz8/cz8
AZ8      = $(PCC_ROOT)/az8/az8
LDZ8     = $(PCC_ROOT)/ldz8
CPP      = cpp
CPPFLAGS = -Dz8000 -Dz8002 -Ih

# Emulator library
EMU_DIR = ../../../z8000_emu
EMU_LIB = $(EMU_DIR)/build/libz8000.a
EMU_INC = $(EMU_DIR)/include
EMU_TOOLS = $(EMU_DIR)/tools

# Tools
TOOLS_DIR = ../../../tools

.PHONY: all clean test emu_lib disk_image

# Handler object files (in build dir)
HANDLER_OBJS = $(B)/krt.b \
    $(B)/sys/main.b $(B)/sys/bio.b $(B)/sys/alloc.b $(B)/sys/iget.b $(B)/sys/nami.b \
    $(B)/sys/rdwri.b $(B)/sys/subr.b $(B)/sys/fio.b $(B)/sys/prf.b $(B)/sys/machdep.b \
    $(B)/sys/slp.b $(B)/sys/trap.b $(B)/sys/sys1.b $(B)/sys/sys2.b $(B)/sys/sys3.b \
    $(B)/sys/sys4.b $(B)/sys/pipe.b $(B)/sys/sig.b $(B)/sys/sysent.b \
    $(B)/sys/clock.b $(B)/sys/prim.b \
    $(B)/dev/md.b $(B)/dev/hd.b $(B)/dev/cons.b $(B)/dev/conf.b \
    $(B)/dev/tty.b $(B)/dev/partab.b

all: $(B)/rom.bin $(B)/kernel.bin $(B)/handler.bin $(B)/test_driver

# =============================================================================
# Ensure emulator library is built
# =============================================================================
emu_lib:
	$(MAKE) -C $(EMU_DIR) libz8000

$(EMU_LIB): emu_lib

# =============================================================================
# ROM binary (segment 0) - assembled in segmented mode
# =============================================================================
$(B)/rom.o $(B)/rom.lst: rom.s | $(B)
	$(Z8K_AS) -z8001 -alns=$(B)/rom.lst -o $(B)/rom.o $<

$(B)/rom.coff: $(B)/rom.o
	$(Z8K_LD) -m z8001 -Ttext=0 -e 0 -o $@ $<

$(B)/rom.bin: $(B)/rom.coff
	$(Z8K_OBJCOPY) -O binary $< $@
	@echo "ROM: $$(wc -c < $@) bytes"

# =============================================================================
# Kernel binary (segment 1) - assembled in segmented mode
# (trap stubs use @RR14 for segmented stack pointer)
# The PSA table uses .word directives for segmented address constants.
# =============================================================================
$(B)/trap.o $(B)/trap.lst: trap.s | $(B)
	$(Z8K_AS) -z8001 -alns=$(B)/trap.lst -o $(B)/trap.o $<

$(B)/trap.coff: $(B)/trap.o
	$(Z8K_LD) -m z8001 -Ttext=0 -e 0 -o $@ $<

$(B)/kernel.bin: $(B)/trap.coff
	$(Z8K_OBJCOPY) -O binary $< $@
	@echo "Kernel: $$(wc -c < $@) bytes"

# =============================================================================
# C handler binary (PCC toolchain) - loaded at segment 1, offset 0x0200
# =============================================================================

# krt.s: copy to .az8 (az8 requires that extension), then assemble
$(B)/krt.az8: krt.s | $(B)
	cp $< $@

$(B)/krt.b: $(B)/krt.az8
	$(AZ8) -o $@ $<

# Pattern rule for kernel C source in sys/
# Pipeline: cpp | cz8 > .az8, az8 > .b
# grep -v filters cz8 warnings (start with ") from the assembly output
$(B)/sys/%.az8: sys/%.c | $(B)/sys
	$(CPP) $(CPPFLAGS) $< | $(CZ8) 2>&1 | grep -v '^"' > $@

$(B)/sys/%.b: $(B)/sys/%.az8
	$(AZ8) -o $@ $<

# Pattern rule for device driver C source in dev/
$(B)/dev/%.az8: dev/%.c | $(B)/dev
	$(CPP) $(CPPFLAGS) $< | $(CZ8) 2>&1 | grep -v '^"' > $@

$(B)/dev/%.b: $(B)/dev/%.az8
	$(AZ8) -o $@ $<

# Link all kernel objects into b.out, then extract flat binary
# Arithmetic library for long multiply/divide/modulus
$(B)/arith.az8: $(PCC_ROOT)/lib/arith.az8 | $(B)
	cp $< $@

$(B)/arith.b: $(B)/arith.az8
	$(AZ8) -o $@ $<

$(B)/handler.bout: $(HANDLER_OBJS) $(B)/arith.b
	$(LDZ8) -x -R 200 $^ -o $@

$(B)/handler.bin: $(B)/handler.bout
	python3 $(TOOLS_DIR)/bout2bin.py $< $@
	@echo "Handler: $$(wc -c < $@) bytes"

# =============================================================================
# Disk image (built by tools)
# =============================================================================
disk_image: $(TOOLS_DIR)/v7mkfs $(TOOLS_DIR)/proto.small $(TOOLS_DIR)/init | $(B)
	cd $(TOOLS_DIR) && ./v7mkfs root.img proto.small
	cp $(TOOLS_DIR)/root.img $(B)/root.img
	cp $(TOOLS_DIR)/root.img $(B)/hd.img

$(TOOLS_DIR)/v7mkfs:
	$(MAKE) -C $(TOOLS_DIR) v7mkfs

$(TOOLS_DIR)/init:
	$(MAKE) -C $(TOOLS_DIR) init

# =============================================================================
# Test driver - links against emulator library
# =============================================================================
$(B)/test_driver: test_driver.cpp $(EMU_LIB) | $(B)
	$(CXX) $(CXXFLAGS) -I$(EMU_INC) -I$(EMU_TOOLS) -o $@ $< -L$(EMU_DIR)/build -lz8000

# =============================================================================
# Build directory creation
# =============================================================================
$(B):
	mkdir -p $(B)

$(B)/sys: | $(B)
	mkdir -p $(B)/sys

$(B)/dev: | $(B)
	mkdir -p $(B)/dev

# =============================================================================
# Test target
# =============================================================================
test: all disk_image
	@echo ""
	@echo "=== Running kernel boot test ==="
	@echo ""
	cd $(B) && ./test_driver -c 200000000

# =============================================================================
# Clean
# =============================================================================
clean:
	rm -rf $(B)
