.sect .text
.sect .rom
.sect .data
.sect .bss
.sect .text

! Test: stack setup, push, call a proc, return, halt.
! Assemble with and without -n to verify segmented vs non-segmented encoding.
!
! Key encodings to verify:
!   push *SP, R0    -> *RR14 (93E0) vs @R15 (93F0)
!   pop  R0, *SP    -> *RR14 (97E0) vs @R15 (97F0)
!   call myproc     -> 2-word DA (seg+off) vs 1-word DA
!   calr myproc     -> same in both (relative)
!   ret             -> same in both (9E08)

start:
	ldl	RR14, $0		! init stack pointer
	push	*SP, $0			! push a zero
	push	*SP, R0			! push R0
	call	myproc			! call with direct address
	pop	R0, *SP			! pop result
	halt

myproc:
	push	*SP, R13		! save frame pointer
	ld	R13, R15		! set up frame
	ld	R0, $7			! R0 = 7
	pop	R13, *SP		! restore frame pointer
	ret
