# Kernel Makefile
# Builds ROM + kernel binaries and a test driver that boots
# the V7 kernel to process 0 with filesystem and console.
#
# All build artifacts go into $(B)/ to keep the source tree clean.

CXX ?= c++
CXXFLAGS = -std=c++17 -Wall -Wextra

# Build directory
B = build

# Z8000 cross-toolchain
Z8K_PREFIX ?= z8k-coff-
Z8K_AS = $(Z8K_PREFIX)as
Z8K_LD = $(Z8K_PREFIX)ld
Z8K_OBJCOPY = $(Z8K_PREFIX)objcopy

# ACK toolchain
ACK_ROOT = ../../../ack/.obj/staging
ACK = ACKDIR=$(ACK_ROOT) $(ACK_ROOT)/bin/ack
ACK_LED = $(ACK_ROOT)/lib/ack/em_led
ASLOD = $(ACK_ROOT)/bin/aslod
ACK_PLATFORM = $(ACK_ROOT)/share/ack/z8000

# Emulator library
EMU_DIR = ../../../z8000_emu
EMU_LIB = $(EMU_DIR)/build/libz8000.a
EMU_INC = $(EMU_DIR)/include
EMU_TOOLS = $(EMU_DIR)/tools

# Tools
TOOLS_DIR = ../../../tools

.PHONY: all clean test emu_lib disk_image

# Handler object files (in build dir)
HANDLER_OBJS = $(B)/krt.o \
    $(B)/sys/main.o $(B)/sys/bio.o $(B)/sys/alloc.o $(B)/sys/iget.o $(B)/sys/nami.o \
    $(B)/sys/rdwri.o $(B)/sys/subr.o $(B)/sys/fio.o $(B)/sys/prf.o $(B)/sys/machdep.o \
    $(B)/sys/slp.o $(B)/sys/trap.o $(B)/sys/sys1.o $(B)/sys/sig.o $(B)/sys/sysent.o \
    $(B)/sys/clock.o $(B)/sys/prim.o \
    $(B)/dev/md.o $(B)/dev/hd.o $(B)/dev/cons.o $(B)/dev/conf.o \
    $(B)/dev/tty.o $(B)/dev/partab.o

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
# C handler binary (ACK toolchain) - loaded at segment 1, offset 0x0200
# =============================================================================

# Assembly files
$(B)/krt.o: krt.s | $(B)
	$(ACK) -mz8000 -z8002 -c -o $@ $<

# Pattern rule for kernel C source in sys/
$(B)/sys/%.o: sys/%.c | $(B)/sys
	$(ACK) -mz8000 -z8002 -c -o $@ $<

# Pattern rule for device driver C source in dev/
$(B)/dev/%.o: dev/%.c | $(B)/dev
	$(ACK) -mz8000 -z8002 -c -o $@ $<

$(B)/handler.out: $(HANDLER_OBJS)
	$(ACK_LED) -a0:2 -a1:2 -a2:2 -a3:2 -b0:0x0200 \
		$^ $(ACK_PLATFORM)/libem.a $(ACK_PLATFORM)/libend.a \
		-o $@

$(B)/handler.bin: $(B)/handler.out
	$(ASLOD) $< $@
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
	cd $(B) && ./test_driver

# =============================================================================
# Clean
# =============================================================================
clean:
	rm -rf $(B)
