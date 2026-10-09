HOSTCC ?= cc
BUILD ?= ../../tests/build/sout-utils-host
FMT = ../asz8k/src
CMD = ../../v7z8000/usr/src/cmd
INC = ../../v7z8000/usr/include
all: $(BUILD)/nm $(BUILD)/size $(BUILD)/strip
$(BUILD)/%: $(CMD)/%.c object.c object.h $(FMT)/soutfmt.c $(FMT)/soutfmt.h
	mkdir -p $(BUILD)
	$(HOSTCC) -std=gnu89 -O2 -fwrapv -w -I. -I$(FMT) -idirafter $(INC) $< object.c $(FMT)/soutfmt.c -o $@
.PHONY: all
