# Kernel Makefile
# Builds ROM + kernel binaries and a test driver that boots
# the V7 kernel to process 0 with filesystem and console.

CXX ?= c++
CXXFLAGS = -std=c++17 -Wall -Wextra

# Z8000 cross-toolchain
Z8K_PREFIX ?= z8k-coff-
Z8K_AS = $(Z8K_PREFIX)as
Z8K_LD = $(Z8K_PREFIX)ld
Z8K_OBJCOPY = $(Z8K_PREFIX)objcopy

# ACK toolchain
ACK_ROOT = ../ack/.obj/staging
ACK = ACKDIR=$(ACK_ROOT) $(ACK_ROOT)/bin/ack
ACK_LED = $(ACK_ROOT)/lib/ack/em_led
ASLOD = $(ACK_ROOT)/bin/aslod
ACK_PLATFORM = $(ACK_ROOT)/share/ack/z8000

# Emulator library
EMU_DIR = ../z8000_emu
EMU_LIB = $(EMU_DIR)/build/libz8000.a
EMU_INC = $(EMU_DIR)/include
EMU_TOOLS = $(EMU_DIR)/tools

# Tools
TOOLS_DIR = ../tools

.PHONY: all clean test emu_lib disk_image

# Handler object files
HANDLER_OBJS = krt.o \
    sys/main.o sys/bio.o sys/alloc.o sys/iget.o sys/nami.o \
    sys/rdwri.o sys/subr.o sys/fio.o sys/prf.o sys/machdep.o \
    dev/md.o dev/cons.o dev/conf.o

all: rom.bin kernel.bin handler.bin test_driver

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
# C handler binary (ACK toolchain) - loaded at segment 1, offset 0x0200
# =============================================================================

# Assembly files
krt.o: krt.s
	$(ACK) -mz8000 -z8002 -c -o $@ $<

# Pattern rule for kernel C source in sys/
sys/%.o: sys/%.c
	@mkdir -p sys
	$(ACK) -mz8000 -z8002 -c -o $@ $<

# Pattern rule for device driver C source in dev/
dev/%.o: dev/%.c
	@mkdir -p dev
	$(ACK) -mz8000 -z8002 -c -o $@ $<

handler.out: $(HANDLER_OBJS)
	$(ACK_LED) -a0:2 -a1:2 -a2:2 -a3:2 -b0:0x0200 \
		$^ $(ACK_PLATFORM)/libem.a $(ACK_PLATFORM)/libend.a \
		-o $@

handler.bin: handler.out
	$(ASLOD) $< $@
	@echo "Handler: $$(wc -c < $@) bytes"

# =============================================================================
# Disk image (built by tools)
# =============================================================================
disk_image: $(TOOLS_DIR)/v7mkfs $(TOOLS_DIR)/proto.small
	cd $(TOOLS_DIR) && ./v7mkfs root.img proto.small
	cp $(TOOLS_DIR)/root.img .

$(TOOLS_DIR)/v7mkfs:
	$(MAKE) -C $(TOOLS_DIR)

# =============================================================================
# Test driver - links against emulator library
# =============================================================================
test_driver: test_driver.cpp $(EMU_LIB)
	$(CXX) $(CXXFLAGS) -I$(EMU_INC) -I$(EMU_TOOLS) -o $@ $< -L$(EMU_DIR)/build -lz8000

# =============================================================================
# Test target
# =============================================================================
test: all disk_image
	@echo ""
	@echo "=== Running kernel boot test ==="
	@echo ""
	./test_driver

# =============================================================================
# Clean
# =============================================================================
clean:
	rm -f *.o *.coff *.bin *.lst *.out test_driver root.img
	rm -f sys/*.o dev/*.o
