HOSTCC ?= cc
BUILD ?= ../../tests/build/ldz8-host
PCC = ../../PCC-z8000/z8000
FMT = ../asz8k/src
all: $(BUILD)/ldz8
$(BUILD)/ldz8: dispatch.c ldso.c $(FMT)/soutfmt.c $(FMT)/soutfmt.h
	mkdir -p $(BUILD)
	$(HOSTCC) -std=gnu89 -O2 -fwrapv -fcommon -w -Wno-return-mismatch -Wno-int-conversion -I$(FMT) dispatch.c ldso.c $(FMT)/soutfmt.c -o $@
.PHONY: all
