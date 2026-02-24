! Test kernel for M20 boot test
! Loaded by bootloader to <2>:0000
! Fills video memory (<3>:0000, 16KB) with 0xAAAA pattern (vertical stripes) and halts

	.segm
	.text
	.global	_start
_start:
	ld	r2, #0x8300		! segment 3 (video memory)
	ld	r3, #0x0000		! offset 0
	ld	r0, #0xAAAA		! pattern: vertical stripes
	ld	r4, #8192		! 16KB / 2 = 8192 words
fill:
	ld	@rr2, r0
	inc	r3, #2
	dec	r4, #1
	jr	nz, fill
	halt
