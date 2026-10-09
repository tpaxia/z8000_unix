HOSTCC ?= cc
BUILD ?= ../../tests/build/sout-utils-host
FMT = ../asz8k/src
INC = ../../v7z8000/usr/include
all: $(BUILD)/nm $(BUILD)/size $(BUILD)/strip
$(BUILD)/%: %.c object.c object.h $(FMT)/soutfmt.c $(FMT)/soutfmt.h
	mkdir -p $(BUILD)
	$(HOSTCC) -std=gnu89 -O2 -fwrapv -w -I$(FMT) -idirafter $(INC) $< object.c $(FMT)/soutfmt.c -o $@
.PHONY: all
