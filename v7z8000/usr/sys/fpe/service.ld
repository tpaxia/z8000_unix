SECTIONS {
 .text 0 : { unix.o(.text) core.o(.text) }
 .data : { unix.o(.data) core.o(.data) }
 .bss : { *(.bss) }
 ASSERT(. < 0xf000, "FPE overlaps per-process stack/u-area")
}
