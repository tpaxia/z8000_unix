Z8KAS  = z8k-coff-as -z8001
Z8KLD  = z8k-coff-ld
Z8KOCP = z8k-coff-objcopy

all: build/hd.img

build/bootloader.bin: bootloader.s | build
	$(Z8KAS) -o build/bootloader.o $<
	$(Z8KLD) -m z8001 -Ttext 0 -o build/bootloader.coff build/bootloader.o
	$(Z8KOCP) -O binary build/bootloader.coff $@

build/testkernel.bin: testkernel.s | build
	$(Z8KAS) -o build/testkernel.o $<
	$(Z8KLD) -m z8001 -Ttext 0 -o build/testkernel.coff build/testkernel.o
	$(Z8KOCP) -O binary build/testkernel.coff $@

build/hd.img: build/bootloader.bin build/testkernel.bin mkm20hd.py
	python3 mkm20hd.py build/bootloader.bin build/testkernel.bin $@

build:
	mkdir -p build

clean:
	rm -rf build

.PHONY: all clean
