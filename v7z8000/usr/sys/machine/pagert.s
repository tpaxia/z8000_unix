! Context restore for the custom paged MMU.
	.text
	.globl _resume
! =============================================================================
! resume(p_addr, label) -- Restore context, return 1
! void resume(int p_addr, label_t label);
!
! Writes KDSA6 (out 0x00B0) to remap the u-area + kernel stack,
! then restores ALL state from label_t (registers, R13 FP, SP, return addr).
! Does NOT depend on stack contents -- stack may have been clobbered
! by bcopy between save() and resume().
!
! No prologue: arguments are read from SP before the stack is remapped.
!
! Interrupts are held off from the remap until SP is restored. In that
! window SP still holds the old process's value but the stack pages already
! belong to the new process, so an interrupt would push its frame over the
! new process's live stack. (V7's PDP-11 resume does the same with
! "bis $340,PS" around the KDSA6 write.)
! =============================================================================
_resume:
	ld	r0, 2(sp)		! p_addr = u-area base frame (1st arg)
	ld	r1, 4(sp)		! label_t pointer (2nd arg)
	ldctl	r3, fcw			! r3 = caller's FCW, restored below
	ld	r2, r3
	and	r2, #0xE7FF		! clear VIE+NVIE
	ldctl	fcw, r2
	out	r0, #0x00B0		! *** KDSA6: remap u-area pages ***
	! Now r1 points into the NEW process's label_t (in remapped u-area).
	ld	r4, 0(r1)		! restore r4-r7
	ld	r5, 2(r1)
	ld	r6, 4(r1)
	ld	r7, 6(r1)
	ld	r10, 8(r1)		! restore r10-r12
	ld	r11, 10(r1)
	ld	r12, 12(r1)
	ld	r14, 14(r1)		! restore r14 (callee-saved)
	ld	r13, 16(r1)		! restore caller's r13 (FP)
	ld	sp, 20(r1)		! restore caller's SP
	ldctl	fcw, r3			! stack is consistent again: allow interrupts
	! Push the return address onto the (now correct) stack and return.
	! This writes 2 bytes below the restored SP -- safe dead zone.
	ld	r2, 18(r1)		! r2 = return address
	ldk	r0, #1			! return value = 1
	push	@sp, r2			! push return address
	ret				! pop return address and jump there
