! Kernel runtime stub for PCC/az8 toolchain.
! Entry points called from trap.s in NONSEG+SYS mode.
!
! Layout at 0x0200 (start of handler.bin):
!   0x0200: jr syscall_dispatch   (2 bytes) - SYSCALL handler calls here
!   0x0202: jr boot_entry         (2 bytes) - boot path calls here
!   0x0204: jr nvi_dispatch       (2 bytes) - NVI handler calls here
!   0x0206: jr vi_dispatch        (2 bytes) - VI handler calls here
!
! PCC calling convention:
!   Frame pointer: R13
!   Stack pointer: R15
!   Callee-saved:  R4-R7, R10-R12, R14
!   Prologue: push @sp, r13 / ld r13, sp
!   Args at:  4(r13), 6(r13), ...

	.text
	.globl	_putchar
	.globl	_inb
	.globl	_inw
	.globl	_insw
	.globl	_outb
	.globl	_outw
	.globl	_outsw
	.globl	_idle
	.globl	_save
	.globl	_resume
	.globl	_retu
	.globl	_set_usp
	.globl	_get_usp
	.globl	_fubyte
	.globl	_subyte
	.globl	_fuword
	.globl	_suword
	.globl	_copyin
	.globl	_copyout
	.globl	_fuibyte
	.globl	_suibyte
	.globl	_copyiin
	.globl	_copyiout
	.globl	_spl0
	.globl	_spl1
	.globl	_spl4
	.globl	_spl5
	.globl	_spl6
	.globl	_spl7
	.globl	_splx
	.globl	_display

! --- Jump table at offset 0x0000 (address 0x0200) ---
	jr	syscall_dispatch	! 0x0200: syscall entry
	jr	boot_entry		! 0x0202: boot entry
	jr	nvi_dispatch		! 0x0204: NVI handler entry (clock)
	jr	vi_dispatch		! 0x0206: VI handler entry (devices)
	jr	epu_dispatch		! 0x0208: SEG call from EPU service

! EPU service supplies a full saved frame in R9. Entered by SEG CALL.
epu_dispatch:
	ld	r0, #0x5800
	ldctl	fcw, r0
	push	@sp, r9
	call	_fptrap
	add	sp, #2
	ld	r0, #0xC000
	ldctl	fcw, r0
	ret

! --- Syscall dispatch entry ---
! trap.s pushes (num, regs) on the stack, calls 0x0200.
! C-callable wrapper around trap().
syscall_dispatch:
	push	@sp, r13
	ld	r13, sp
	ld	r0, 4(r13)		! num
	ld	r1, 6(r13)		! regs
	sub	sp, #4
	ld	2(sp), r1
	ld	0(sp), r0
	calr	_trap
	add	sp, #4
	ld	sp, r13
	pop	r13, @sp
	ret

! --- NVI dispatch entry (clock) ---
! Called from trap.s nvi_entry in NONSEG+SYS mode.
! R0 = interrupted FCW (passed by nvi_entry from IRET frame).
! Calls clock(ps) where ps = interrupted FCW.
nvi_dispatch:
	push	@sp, r13
	ld	r13, sp
	sub	sp, #2
	ld	0(sp), r0		! push argument: ps = interrupted FCW
	calr	_clock
	add	sp, #2
	jr	irq_return

! --- VI dispatch entry (device interrupts) ---
! Called from trap.s vi_entry in NONSEG+SYS mode.
! R0 = vector identifier (from tag word on IRET frame).
! All devices share VI vector 0.  Each handler guards itself:
!   hdintr checks hd_bp==0, consrint checks RX-ready status.
vi_dispatch:
	push	@sp, r13
	ld	r13, sp
	calr	_hdintr
	calr	_consrint
irq_return:
	! Saved R0 starts four bytes above our frame pointer (saved R13,
	! return address). intrret only schedules when returning to user mode.
	ld	r0, r13
	add	r0, #4
	push	@sp, r0
	calr	_intrret
	add	sp, #2
	! Mask before restoring the wrapper frame and entering the SEG epilog.
	ldctl	r1, fcw
	and	r1, #0xE7FF
	ldctl	fcw, r1
	ld	sp, r13
	pop	r13, @sp
	ret

! --- Boot entry ---
boot_entry:
	lda	r2, _edata
	lda	r3, _end
.Lbz1:	cp	r2, r3
	jr ge,	.Lbz2
	clr	@r2
	inc	r2, #2
	jr	.Lbz1
.Lbz2:
	! Kernel stack at top of u-area page (0xF000-0xFFFF).
	! MMU maps these pages per-process via KDSA6.
	ld	sp, #0xFFFE
	! Enable VIE only: set FCW to NONSEG+SYS+VIE (0x5000)
	! NVIE (clock) enabled later by clkstart() after proc[0] setup.
	ld	r0, #0x5000
	ldctl	fcw, r0
	calr	_main
	! After main returns in child process, enter user mode
	jp	_retu

! --- void putchar(int ch) ---
_putchar:
	push	@sp, r13
	ld	r13, sp
	ld	r1, 4(r13)
	outb	rl1, #0x00F0
	ld	sp, r13
	pop	r13, @sp
	ret

! --- int inb(int port) ---
_inb:
	push	@sp, r13
	ld	r13, sp
	ld	r2, 4(r13)
	inb	rl0, @r2
	and	r0, #0x00FF
	ld	sp, r13
	pop	r13, @sp
	ret

! --- void outb(int port, int byte) ---
_outb:
	push	@sp, r13
	ld	r13, sp
	ld	r2, 4(r13)
	ld	r3, 6(r13)
	outb	rl3, @r2
	ld	sp, r13
	pop	r13, @sp
	ret

! --- void idle(void) ---
! Called from swtch() with interrupts disabled (spl6).
! Enable VIE+NVIE so clock/device interrupts can wake us from HALT.
! After interrupt handler returns via IRET, we resume here with
! the FCW restored (interrupts enabled), then return to swtch().
_idle:
	ldctl	r0, fcw
	or	r0, #0x1800		! set VIE+NVIE
	ldctl	fcw, r0
	halt
	ret

! =============================================================================
! save(label) -- Save context, return 0
! int save(label_t label);
!
! PCC callee-saved: R4-R7, R10-R12, R14.  Frame pointer: R13.
! Saves all callee-saved regs + caller's R13 (FP) + ret addr + caller's SP
! into label_t[12].  resume() restores entirely from the label_t without
! depending on the stack contents -- so the u-area copy in newproc()
! can safely clobber save()'s old stack frame.
! =============================================================================
_save:
	push	@sp, r13
	ld	r13, sp
	ld	r1, 4(r13)		! r1 = label_t pointer (argument)
	ld	0(r1), r4		! label[0] = r4
	ld	2(r1), r5		! label[1] = r5
	ld	4(r1), r6		! label[2] = r6
	ld	6(r1), r7		! label[3] = r7
	ld	8(r1), r10		! label[4] = r10
	ld	10(r1), r11		! label[5] = r11
	ld	12(r1), r12		! label[6] = r12
	ld	14(r1), r14		! label[7] = r14 (callee-saved)
	! Save caller's R13 (FP, pushed on stack by our prologue)
	ld	r0, 0(r13)		! r0 = caller's r13 (FP)
	ld	16(r1), r0		! label[8] = caller's r13 (FP)
	! Save return address (pushed by calr)
	ld	r0, 2(r13)		! r0 = return address
	ld	18(r1), r0		! label[9] = return address
	! Save caller's SP: r13 + 6 (skip pushed r13 + ret addr + argument)
	ld	r0, r13
	add	r0, #6
	ld	20(r1), r0		! label[10] = caller's SP
	clr	r0			! return 0
	ld	sp, r13
	pop	r13, @sp
	ret

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

! =============================================================================
! Cross-segment memory access functions.
!
! These temporarily switch to SEG+SYS mode for segmented memory access.
! In SEG mode, @rr2 (indirect via register pair r2:r3) gives segmented
! addressing: r2=segment encoding, r3=offset.
!
! The Z8001 CPU decodes register-indirect (IR) mode identically in both
! SEG and NONSEG modes -- only base-address (BA/DA) instructions differ.
! So we can assemble in z8002 mode and the IR instructions work in both.
!
! CRITICAL: No BA/DA/X-mode instructions while in SEG mode!
! The CPU would try to decode them with 6-byte segmented format.
! =============================================================================

! --- int fubyte(addr) ---
_fubyte:
	push	@sp, r13
	ld	r13, sp
	ld	r2, _useg		! user segment encoding
	ld	r3, 4(r13)		! user offset
	ldctl	r9, fcw			! preserve caller interrupt state
	ld	r0, #0xC000
	ldctl	fcw, r0			! SEG+SYS
	! --- SEG mode: only IR/reg/imm instructions ---
	ldb	rl0, @r2		! load byte from seg:off
	ldctl	fcw, r9			! NONSEG+SYS
	! --- back to NONSEG mode ---
	and	r0, #0x00FF		! zero-extend
	ld	sp, r13
	pop	r13, @sp
	ret

! --- int subyte(addr, val) ---
_subyte:
	push	@sp, r13
	ld	r13, sp
	ld	r2, _useg
	ld	r3, 4(r13)		! user offset
	ld	r0, 6(r13)		! value in R0 (rl0 for byte)
	ldctl	r9, fcw			! preserve caller interrupt state
	ld	r1, #0xC000
	ldctl	fcw, r1			! SEG+SYS
	ldb	@r2, rl0		! store byte to seg:off
	ldctl	fcw, r9			! NONSEG+SYS
	ldk	r0, #0
	ld	sp, r13
	pop	r13, @sp
	ret

! --- int fuword(addr) ---
_fuword:
	push	@sp, r13
	ld	r13, sp
	ld	r2, _useg
	ld	r3, 4(r13)
	ldctl	r9, fcw			! preserve caller interrupt state
	ld	r0, #0xC000
	ldctl	fcw, r0			! SEG+SYS
	ld	r0, @r2			! load word from seg:off
	ldctl	fcw, r9			! NONSEG+SYS
	ld	sp, r13
	pop	r13, @sp
	ret

! --- int suword(addr, val) ---
_suword:
	push	@sp, r13
	ld	r13, sp
	ld	r2, _useg
	ld	r3, 4(r13)
	ld	r8, 6(r13)		! value (R8 caller-saved)
	ldctl	r9, fcw			! preserve caller interrupt state
	ld	r0, #0xC000
	ldctl	fcw, r0			! SEG+SYS
	ld	@r2, r8			! store word to seg:off
	ldctl	fcw, r9			! NONSEG+SYS
	ldk	r0, #0
	ld	sp, r13
	pop	r13, @sp
	ret

! =============================================================================
! copyin(from_user, to_kernel, count)
! =============================================================================
_copyin:
	push	@sp, r13
	ld	r13, sp
	ld	r2, _useg		! user segment encoding
	ld	r3, 4(r13)		! from: user offset
	ld	r8, 6(r13)		! to: kernel address (R8 caller-saved)
	ld	r9, 8(r13)		! count (R9 caller-saved)
	cp	r9, #0
	jr eq,	.Lcidone
.Lciloop:
	ldctl	r1, fcw			! restore caller state after each byte
	ld	r0, #0xC000
	ldctl	fcw, r0			! SEG+SYS
	ldb	rl0, @r2		! load byte from user space
	ldctl	fcw, r1			! NONSEG+SYS
	ldb	@r8, rl0		! store byte to kernel
	inc	r3, #1			! advance user offset
	inc	r8, #1			! advance kernel pointer
	dec	r9, #1
	jr ne,	.Lciloop
.Lcidone:
	ldk	r0, #0
	ld	sp, r13
	pop	r13, @sp
	ret

! =============================================================================
! copyout(from_kernel, to_user, count)
! =============================================================================
_copyout:
	push	@sp, r13
	ld	r13, sp
	push	@sp, r4			! save R4 (callee-saved, used for FCW)
	push	@sp, r5
	ldctl	r4, fcw			! caller state, restored after each byte
	ld	r5, #0xC000
	ld	r2, 4(r13)		! from: kernel address
	ld	r3, 6(r13)		! to: user offset
	ld	r1, 8(r13)		! count (R1 caller-saved)
	ld	r8, _useg		! R8 = segment (R8 caller-saved, constant)
	cp	r1, #0
	jr eq,	.Lcodone
.Lcoloop:
	ldb	rl0, @r2		! load byte from kernel
	inc	r2, #1			! advance kernel pointer
	ld	r9, r3			! R9 = user offset copy
	ldctl	fcw, r5			! SEG+SYS
	ldb	@r8, rl0		! store byte to user space via @RR8
	ldctl	fcw, r4			! NONSEG+SYS
	inc	r3, #1			! advance user offset
	dec	r1, #1
	jr ne,	.Lcoloop
.Lcodone:
	ldk	r0, #0
	pop	r5, @sp
	pop	r4, @sp			! restore R4
	ld	sp, r13
	pop	r13, @sp
	ret

! Instruction-space user memory helpers (iseg selected by sureg).
_fuibyte:
	push	@sp, r13
	ld	r13, sp
	ld	r2, _iseg		! user segment encoding
	ld	r3, 4(r13)		! user offset
	ldctl	r9, fcw			! preserve caller interrupt state
	ld	r0, #0xC000
	ldctl	fcw, r0			! SEG+SYS
	! --- SEG mode: only IR/reg/imm instructions ---
	ldb	rl0, @r2		! load byte from seg:off
	ldctl	fcw, r9			! NONSEG+SYS
	! --- back to NONSEG mode ---
	and	r0, #0x00FF		! zero-extend
	ld	sp, r13
	pop	r13, @sp
	ret

! --- int suibyte(addr, val) ---
_suibyte:
	push	@sp, r13
	ld	r13, sp
	ld	r2, _iseg
	ld	r3, 4(r13)		! user offset
	ld	r0, 6(r13)		! value in R0 (rl0 for byte)
	ldctl	r9, fcw			! preserve caller interrupt state
	ld	r1, #0xC000
	ldctl	fcw, r1			! SEG+SYS
	ldb	@r2, rl0		! store byte to seg:off
	ldctl	fcw, r9			! NONSEG+SYS
	ldk	r0, #0
	ld	sp, r13
	pop	r13, @sp
	ret

! --- copyiin(from_user, to_kernel, count) ---
_copyiin:
	push	@sp, r13
	ld	r13, sp
	ld	r2, _iseg		! user segment encoding
	ld	r3, 4(r13)		! from: user offset
	ld	r8, 6(r13)		! to: kernel address (R8 caller-saved)
	ld	r9, 8(r13)		! count (R9 caller-saved)
	cp	r9, #0
	jr eq,	.Lcidone_i
.Lciloop_i:
	ldctl	r1, fcw			! restore caller state after each byte
	ld	r0, #0xC000
	ldctl	fcw, r0			! SEG+SYS
	ldb	rl0, @r2		! load byte from user space
	ldctl	fcw, r1			! NONSEG+SYS
	ldb	@r8, rl0		! store byte to kernel
	inc	r3, #1			! advance user offset
	inc	r8, #1			! advance kernel pointer
	dec	r9, #1
	jr ne,	.Lciloop_i
.Lcidone_i:
	ldk	r0, #0
	ld	sp, r13
	pop	r13, @sp
	ret

! =============================================================================
! copyiout(from_kernel, to_user, count)
! =============================================================================
_copyiout:
	push	@sp, r13
	ld	r13, sp
	push	@sp, r4			! save R4 (callee-saved, used for FCW)
	push	@sp, r5
	ldctl	r4, fcw			! caller state, restored after each byte
	ld	r5, #0xC000
	ld	r2, 4(r13)		! from: kernel address
	ld	r3, 6(r13)		! to: user offset
	ld	r1, 8(r13)		! count (R1 caller-saved)
	ld	r8, _iseg		! R8 = segment (R8 caller-saved, constant)
	cp	r1, #0
	jr eq,	.Lcodone_i
.Lcoloop_i:
	ldb	rl0, @r2		! load byte from kernel
	inc	r2, #1			! advance kernel pointer
	ld	r9, r3			! R9 = user offset copy
	ldctl	fcw, r5			! SEG+SYS
	ldb	@r8, rl0		! store byte to user space via @RR8
	ldctl	fcw, r4			! NONSEG+SYS
	inc	r3, #1			! advance user offset
	dec	r1, #1
	jr ne,	.Lcoloop_i
.Lcodone_i:
	ldk	r0, #0
	pop	r5, @sp
	pop	r4, @sp			! restore R4
	ld	sp, r13
	pop	r13, @sp
	ret

! =============================================================================
! retu() -- Enter user mode.
!
! Builds an IRET frame to transition to NONSEG+NORM in the user segment.
! Must avoid BA/DA mode instructions while in SEG mode.
! =============================================================================
_retu:
	! Stack segment preparation must be atomic with respect to interrupts.
	ldctl	r0, fcw
	and	r0, #0xE7FF
	ldctl	fcw, r0
	! Set up registers for the NONSEG+SYS to SEG+SYS transition.
	! CHANGE_FCW swaps R14 with NSPSEG when the SEG bit changes.
	! We want R14=0x8100 (kernel seg) after the swap so the system
	! stack is in the kernel segment.  Put the kernel seg encoding
	! in NSPSEG (it will be swapped INTO R14) and the user seg
	! encoding in R14 (it will be swapped INTO NSPSEG for later
	! use when IRET transitions to user mode).
	ld	r1, _useg		! user segment encoding (e.g. 0x8200)
	ld	r14, r1			! r14 = user seg (will go to NSPSEG)
	ld	r0, #0x8100		! kernel segment encoding
	ldctl	nspseg, r0		! NSPSEG = kernel seg (will go to R14)
	ld	r0, #0xFFF0
	ldctl	nspoff, r0		! user stack offset

	! Switch to SEG+SYS: R14(useg) swapped with NSPSEG(0x8100)
	! After: R14=0x8100, NSPSEG=useg, RR14=kernel system stack
	ld	r0, #0xC000
	ldctl	fcw, r0

	! --- SEG mode: only IR/reg/imm instructions! ---
	! Build IRET frame on system stack (@rr14):
	!   push PC (4 bytes): user_seg:0x0000
	!   push FCW (2 bytes): 0x1800 (NONSEG+NORM, interrupts enabled)
	!   push tag (2 bytes): 0x0000
	clr	r0
	push	@r14, r0		! PC low: offset 0
	ld	r0, r1			! r1 = useg (loaded before mode switch)
	push	@r14, r0		! PC high: user segment encoding
	ld	r0, #0x1800
	push	@r14, r0		! FCW: NONSEG+NORM + VIE + NVIE
	clr	r0
	push	@r14, r0		! tag: 0x0000

	! Clear user registers
	clr	r0
	clr	r1
	clr	r2
	clr	r3
	clr	r4
	clr	r5
	clr	r6
	clr	r7
	clr	r8
	clr	r9
	clr	r10
	clr	r11
	clr	r12

	iret				! -> NONSEG+NORM, user_seg:0x0000


! --- void set_usp(int value) ---
! Set user stack pointer (NSPOFF control register).
_set_usp:
	push	@sp, r13
	ld	r13, sp
	ld	r0, 4(r13)		! user SP value
	ldctl	nspoff, r0
	ld	sp, r13
	pop	r13, @sp
	ret

! --- int get_usp(void) ---
! Read user stack pointer (NSPOFF control register).
_get_usp:
	ldctl	r0, nspoff
	ret

! --- int inw(int port) ---
! Word-width I/O input, used for ATA data register.
_inw:
	push	@sp, r13
	ld	r13, sp
	ld	r2, 4(r13)		! port
	in	r0, @r2
	ld	sp, r13
	pop	r13, @sp
	ret

! --- void insw(int port, char *addr, int count) ---
! Block word input: read count words from I/O port to memory.
_insw:
	push	@sp, r13
	ld	r13, sp
	ld	r2, 4(r13)		! port
	ld	r8, 6(r13)		! addr (R8 caller-saved)
	ld	r9, 8(r13)		! count (R9 caller-saved)
	cp	r9, #0
	jr eq,	.Liswdone
.Liswloop:
	in	r0, @r2			! read word from port
	ld	@r8, r0			! store to memory
	inc	r8, #2			! advance pointer by word
	dec	r9, #1
	jr ne,	.Liswloop
.Liswdone:
	ld	sp, r13
	pop	r13, @sp
	ret

! --- void outsw(int port, char *addr, int count) ---
! Block word output: write count words from memory to I/O port.
_outsw:
	push	@sp, r13
	ld	r13, sp
	ld	r2, 4(r13)		! port
	ld	r8, 6(r13)		! addr (R8 caller-saved)
	ld	r9, 8(r13)		! count (R9 caller-saved)
	cp	r9, #0
	jr eq,	.Loswdone
.Loswloop:
	ld	r0, @r8			! load word from memory
	out	r0, @r2			! write word to port
	inc	r8, #2			! advance pointer by word
	dec	r9, #1
	jr ne,	.Loswloop
.Loswdone:
	ld	sp, r13
	pop	r13, @sp
	ret

! --- void outw(int port, int value) ---
! Word-width I/O output, used for KDSA6 and WPAGE ports.
_outw:
	push	@sp, r13
	ld	r13, sp
	ld	r2, 4(r13)		! port
	ld	r3, 6(r13)		! value (R3 caller-saved)
	out	r3, @r2
	ld	sp, r13
	pop	r13, @sp
	ret

! =============================================================================
! SPL functions -- interrupt priority level control.
!
! Z8000 has no priority levels; VIE (0x1000) and NVIE (0x0800) control
! device and clock interrupts respectively.
! spl0/spl1: enable both (set VIE+NVIE)
! spl4/spl5: disable devices (clear VIE), enable clock (set NVIE)
! spl6/spl7: disable both (clear VIE+NVIE)
! splx(s): restore VIE+NVIE from saved FCW value
! All return the previous FCW value (for splx restoration).
! =============================================================================

! --- spl0/spl1: enable VIE+NVIE (allow all interrupts) ---
_spl0:
_spl1:
	ldctl	r0, fcw			! r0 = old FCW (return value)
	ld	r1, r0
	or	r1, #0x1800		! set VIE+NVIE
	ldctl	fcw, r1
	ret

! --- spl4/spl5: block devices, allow the clock ---
! Like PDP-11 spl5, this sets a level rather than only raising it.
! Device handlers may admit the clock, but must never re-enable VIE.
! In clock(), lowering to this level permits nested ticks during callouts;
! their saved FCW has VIE clear, so BASEPRI defers nested callout execution.
_spl4:
_spl5:
	ldctl	r0, fcw			! r0 = old FCW (return value)
	ld	r1, r0
	and	r1, #0xEFFF		! clear VIE (block devices)
	or	r1, #0x0800		! set NVIE (allow clock)
	ldctl	fcw, r1
	ret

! --- spl6/spl7: disable VIE+NVIE ---
_spl6:
_spl7:
	ldctl	r0, fcw			! r0 = old FCW (return value)
	ld	r1, r0
	and	r1, #0xE7FF		! clear VIE+NVIE
	ldctl	fcw, r1
	ret

! --- splx(s): restore VIE+NVIE from argument ---
_splx:
	ldctl	r0, fcw			! r0 = old FCW (return value)
	ld	r1, r0
	and	r1, #0xE7FF		! clear VIE+NVIE in current
	ld	r2, 2(sp)		! r2 = argument (saved FCW)
	and	r2, #0x1800		! isolate VIE+NVIE bits
	or	r1, r2			! copy VIE+NVIE from argument
	ldctl	fcw, r1
	ret

! --- void display(void) ---
! PDP-11 front panel display -- no-op on Z8000.
_display:
	ret

! --- int test_slal_rl(void) ---
! Test SLAL + RL instructions.
! SLAL rr2 with MSB=1 should set carry.
! RL r1 should shift carry into bit 0.
! Returns 1 if working, 0 if broken.
	.globl	_test_slal_rl
_test_slal_rl:
	ld	r2, #0x8000		! RR2 = 0x80000000
	ld	r3, #0x0000
	clr	r0
	clr	r1
	slal	rr2, #1			! MSB was 1 -> carry=1, RR2=0x00000000
	rl	r1, #1			! carry(1) -> bit 0 of R1: R1=1
	ld	r0, r1			! return R1
	ret

! --- int test_div_hw(void) ---
! Test hardware div instruction: 42 / 10 = 4
	.globl	_test_div_hw
_test_div_hw:
	clr	r0
	ld	r1, #42
	div	rr0, #10		! R1=quotient=4, R0=remainder=2
	ld	r0, r1			! return quotient
	ret

! fprun(frame, workspace, user-D, user-I): call segment 127 offset 0x80.
	.globl _fprun
_fprun:
	push @sp,r13
	ld r13,sp
	push @sp,r4
	push @sp,r5
	push @sp,r6
	push @sp,r7
	push @sp,r10
	push @sp,r11
	push @sp,r12
	push @sp,r14
	ld r9,4(r13)
	ld r10,8(r13)
	ld r11,10(r13)
	ld r13,6(r13)
	ld r0,#0xC000
	ldctl fcw,r0
	.word 0x5f00,0xff00,0x0080
	ld r1,#0x5800
	ldctl fcw,r1
	pop r14,@sp
	pop r12,@sp
	pop r11,@sp
	pop r10,@sp
	pop r7,@sp
	pop r6,@sp
	pop r5,@sp
	pop r4,@sp
	pop r13,@sp
	ret
