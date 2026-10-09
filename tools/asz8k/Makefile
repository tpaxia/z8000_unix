# Host build of the same sources used by src/makefile inside V7.
HOSTCC ?= cc
HOSTFLAGS ?= -std=gnu89 -O2 -fwrapv -fsigned-char -fcommon -w -Wno-int-conversion -Wno-return-mismatch
BUILD ?= ../../tests/build/asz8k-host
SOURCES = $(wildcard src/*.c)
HEADERS = $(wildcard src/*.h)

all: $(BUILD)/asz8k

# Historical object oracle is separate from the installed s.out assembler.
oracle: $(BUILD)/asz8k-oracle
$(BUILD)/asz8k-oracle: $(SOURCES) $(HEADERS) Makefile
	mkdir -p $(BUILD)
	$(HOSTCC) $(HOSTFLAGS) -DASZ_HOST -DASZ_LEGACY $(SOURCES) -o $@

$(BUILD)/asz8k: $(SOURCES) $(HEADERS) Makefile
	mkdir -p $(BUILD)
	$(HOSTCC) $(HOSTFLAGS) -DASZ_HOST $(SOURCES) -o $@

test: all
	python3 ../kernel-asm/test.py --host-only

.PHONY: all test oracle
