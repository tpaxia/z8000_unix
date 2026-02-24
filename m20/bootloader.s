! M20 bootloader
! Loaded by BIOS SAV loader into <6>:0000 (user memory, safe from boot_final)
! Reads test kernel from HD sector 64 into <2>:1000, jumps to it
! (Buffer must be above <2>:0040 to avoid clobbering BIOS workspace)

	.segm
	.text
	.global	_start

KERNEL_SECTOR	= 64
KERNEL_SECTORS	= 2
ROM_DISK_IO	= 0x84000068

_start:
	! Set up stack in segment 2 (DRAM, D=I mapped under PROM MMU)
	ld	r14, #0x8200
	ld	r15, #0xFF00

	! Read test kernel from HD using BIOS disk_io
	ld	r7, #0x0A00		! device=HD(10), opcode=read(0)
	ld	r8, #KERNEL_SECTORS
	ld	r9, #KERNEL_SECTOR
	ld	r10, #0x8200		! buffer segment 2
	ld	r11, #0x1000		! buffer offset 0x1000 (above BIOS workspace)
	ld	r12, #0x8400		! ROM segment 4
	ld	r13, #0x0068		! disk_io offset
	call	@rr12			! call BIOS disk_io

	! Check error
	testb	rl7
	jr	nz, error

	! Jump to loaded kernel at <2>:1000
	ld	r12, #0x8200
	ld	r13, #0x1000
	jp	@rr12

error:
	halt
