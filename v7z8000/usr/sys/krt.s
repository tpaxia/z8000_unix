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
	.globl	putchar
	.globl	inb
	.globl	inw
	.globl	insw
	.globl	outb
	.globl	outw
	.globl	outsw
	.globl	idle
	.globl	save
	.globl	resume
	.globl	retu
	.globl	set_usp
	.globl	get_usp
	.globl	fubyte
	.globl	subyte
	.globl	fuword
	.globl	suword
	.globl	copyin
	.globl	copyout
	.globl	spl0
	.globl	spl1
	.globl	spl4
	.globl	spl5
	.globl	spl6
	.globl	spl7
	.globl	splx
	.globl	display

! --- Jump table at offset 0x0000 (address 0x0200) ---
	jr	syscall_dispatch	! 0x0200: syscall entry
	jr	boot_entry		! 0x0202: boot entry
	jr	nvi_dispatch		! 0x0204: NVI handler entry (clock)
	jr	vi_dispatch		! 0x0206: VI handler entry (devices)

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
	calr	trap
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
	calr	clock
	add	sp, #2
	! clock() may have called spl1() which re-enabled NVIE.
	! Disable VIE+NVIE before returning to trap.s epilog to prevent
	! nested NVI during register restore / IRET sequence.
	! IRET will atomically restore the interrupted FCW (with NVIE set).
	ldctl	r1, fcw
	and	r1, #0xE7FF		! clear VIE+NVIE
	ldctl	fcw, r1
	ld	sp, r13
	pop	r13, @sp
	ret

! --- VI dispatch entry (device interrupts) ---
! Called from trap.s vi_entry in NONSEG+SYS mode.
! R0 = vector identifier (from tag word on IRET frame).
! All devices share VI vector 0.  Each handler guards itself:
!   hdintr checks hd_bp==0, consrint checks RX-ready status.
vi_dispatch:
	push	@sp, r13
	ld	r13, sp
	calr	hdintr
	calr	consrint
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
	calr	main
	! debug: did main() return?
	ld	r0, #0x4000		! NONSEG+SYS, no VIE/NVIE
	ldctl	fcw, r0			! disable all interrupts
	ld	r0, #0x004D		! 'M'
	outb	rl0, #0x00F0
	ld	r0, #0x0031		! '1'
	outb	rl0, #0x00F0
	ld	r0, #0x0032		! '2'
	outb	rl0, #0x00F0
	ld	r0, #0x0033		! '3'
	outb	rl0, #0x00F0
	ld	r0, #0x000A		! '\n'
	outb	rl0, #0x00F0
	! After main returns in child process, enter user mode
	jp	retu
	halt

! --- void putchar(int ch) ---
putchar:
	push	@sp, r13
	ld	r13, sp
	ld	r1, 4(r13)
	outb	rl1, #0x00F0
	ld	sp, r13
	pop	r13, @sp
	ret

! --- int inb(int port) ---
inb:
	push	@sp, r13
	ld	r13, sp
	ld	r2, 4(r13)
	inb	rl0, @r2
	and	r0, #0x00FF
	ld	sp, r13
	pop	r13, @sp
	ret

! --- void outb(int port, int byte) ---
outb:
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
idle:
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
save:
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
! =============================================================================
resume:
	ld	r0, 2(sp)		! p_addr = u-area base frame (1st arg)
	ld	r1, 4(sp)		! label_t pointer (2nd arg)
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
fubyte:
	push	@sp, r13
	ld	r13, sp
	ld	r2, useg		! user segment encoding
	ld	r3, 4(r13)		! user offset
	ld	r0, #0xC000
	ldctl	fcw, r0			! SEG+SYS
	! --- SEG mode: only IR/reg/imm instructions ---
	ldb	rl0, @r2		! load byte from seg:off
	ld	r1, #0x4000
	ldctl	fcw, r1			! NONSEG+SYS
	! --- back to NONSEG mode ---
	and	r0, #0x00FF		! zero-extend
	ld	sp, r13
	pop	r13, @sp
	ret

! --- int subyte(addr, val) ---
subyte:
	push	@sp, r13
	ld	r13, sp
	ld	r2, useg
	ld	r3, 4(r13)		! user offset
	ld	r0, 6(r13)		! value in R0 (rl0 for byte)
	ld	r1, #0xC000
	ldctl	fcw, r1			! SEG+SYS
	ldb	@r2, rl0		! store byte to seg:off
	ld	r1, #0x4000
	ldctl	fcw, r1			! NONSEG+SYS
	ldk	r0, #0
	ld	sp, r13
	pop	r13, @sp
	ret

! --- int fuword(addr) ---
fuword:
	push	@sp, r13
	ld	r13, sp
	ld	r2, useg
	ld	r3, 4(r13)
	ld	r0, #0xC000
	ldctl	fcw, r0			! SEG+SYS
	ld	r0, @r2			! load word from seg:off
	ld	r1, #0x4000
	ldctl	fcw, r1			! NONSEG+SYS
	ld	sp, r13
	pop	r13, @sp
	ret

! --- int suword(addr, val) ---
suword:
	push	@sp, r13
	ld	r13, sp
	ld	r2, useg
	ld	r3, 4(r13)
	ld	r8, 6(r13)		! value (R8 caller-saved)
	ld	r0, #0xC000
	ldctl	fcw, r0			! SEG+SYS
	ld	@r2, r8			! store word to seg:off
	ld	r1, #0x4000
	ldctl	fcw, r1			! NONSEG+SYS
	ldk	r0, #0
	ld	sp, r13
	pop	r13, @sp
	ret

! =============================================================================
! copyin(from_user, to_kernel, count)
! =============================================================================
copyin:
	push	@sp, r13
	ld	r13, sp
	ld	r2, useg		! user segment encoding
	ld	r3, 4(r13)		! from: user offset
	ld	r8, 6(r13)		! to: kernel address (R8 caller-saved)
	ld	r9, 8(r13)		! count (R9 caller-saved)
	cp	r9, #0
	jr eq,	.Lcidone
.Lciloop:
	ld	r0, #0xC000
	ldctl	fcw, r0			! SEG+SYS
	ldb	rl0, @r2		! load byte from user space
	ld	r1, #0x4000
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
copyout:
	push	@sp, r13
	ld	r13, sp
	push	@sp, r4			! save R4 (callee-saved, used for FCW)
	ld	r2, 4(r13)		! from: kernel address
	ld	r3, 6(r13)		! to: user offset
	ld	r1, 8(r13)		! count (R1 caller-saved)
	ld	r8, useg		! R8 = segment (R8 caller-saved, constant)
	cp	r1, #0
	jr eq,	.Lcodone
.Lcoloop:
	ldb	rl0, @r2		! load byte from kernel
	inc	r2, #1			! advance kernel pointer
	ld	r9, r3			! R9 = user offset copy
	ld	r4, #0xC000
	ldctl	fcw, r4			! SEG+SYS
	ldb	@r8, rl0		! store byte to user space via @RR8
	ld	r4, #0x4000
	ldctl	fcw, r4			! NONSEG+SYS
	inc	r3, #1			! advance user offset
	dec	r1, #1
	jr ne,	.Lcoloop
.Lcodone:
	ldk	r0, #0
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
retu:
	outb	rl0, #0x00F0		! debug: 'R' marker (r0 still has old value)
	ld	r0, #0x0052		! 'R'
	outb	rl0, #0x00F0
	ld	r0, #0x000A		! '\n'
	outb	rl0, #0x00F0
	! Set up registers for the NONSEG+SYS to SEG+SYS transition.
	! CHANGE_FCW swaps R14 with NSPSEG when the SEG bit changes.
	! We want R14=0x8100 (kernel seg) after the swap so the system
	! stack is in the kernel segment.  Put the kernel seg encoding
	! in NSPSEG (it will be swapped INTO R14) and the user seg
	! encoding in R14 (it will be swapped INTO NSPSEG for later
	! use when IRET transitions to user mode).
	ld	r1, useg		! user segment encoding (e.g. 0x8200)
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
	!   push FCW (2 bytes): 0x0000 (NONSEG+NORM)
	!   push tag (2 bytes): 0x0000
	clr	r0
	push	@r14, r0		! PC low: offset 0
	ld	r0, r1			! r1 = useg (loaded before mode switch)
	push	@r14, r0		! PC high: user segment encoding
	clr	r0
	push	@r14, r0		! FCW: NONSEG+NORM = 0x0000
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
set_usp:
	push	@sp, r13
	ld	r13, sp
	ld	r0, 4(r13)		! user SP value
	ldctl	nspoff, r0
	ld	sp, r13
	pop	r13, @sp
	ret

! --- int get_usp(void) ---
! Read user stack pointer (NSPOFF control register).
get_usp:
	ldctl	r0, nspoff
	ret

! --- int inw(int port) ---
! Word-width I/O input, used for ATA data register.
inw:
	push	@sp, r13
	ld	r13, sp
	ld	r2, 4(r13)		! port
	in	r0, @r2
	ld	sp, r13
	pop	r13, @sp
	ret

! --- void insw(int port, char *addr, int count) ---
! Block word input: read count words from I/O port to memory.
insw:
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
outsw:
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
outw:
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
! spl0/spl1/spl4/spl5: enable both (set VIE+NVIE)
! spl6/spl7: disable both (clear VIE+NVIE)
! splx(s): restore VIE+NVIE from saved FCW value
! All return the previous FCW value (for splx restoration).
! =============================================================================

! --- spl0/spl1: enable VIE+NVIE (allow all interrupts) ---
spl0:
spl1:
	ldctl	r0, fcw			! r0 = old FCW (return value)
	ld	r1, r0
	or	r1, #0x1800		! set VIE+NVIE
	ldctl	fcw, r1
	ret

! --- spl4/spl5: enable VIE only (allow device interrupts, block clock NVI) ---
! On PDP-11, spl5 blocks clock (priority 6) but allows devices (priority 5).
! On Z8000, NVIE = clock, VIE = devices.
spl4:
spl5:
	ldctl	r0, fcw			! r0 = old FCW (return value)
	ld	r1, r0
	and	r1, #0xF7FF		! clear NVIE (block clock)
	or	r1, #0x1000		! set VIE (allow devices)
	ldctl	fcw, r1
	ret

! --- spl6/spl7: disable VIE+NVIE ---
spl6:
spl7:
	ldctl	r0, fcw			! r0 = old FCW (return value)
	ld	r1, r0
	and	r1, #0xE7FF		! clear VIE+NVIE
	ldctl	fcw, r1
	ret

! --- splx(s): restore VIE+NVIE from argument ---
splx:
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
display:
	ret

! --- int test_slal_rl(void) ---
! Test SLAL + RL instructions.
! SLAL rr2 with MSB=1 should set carry.
! RL r1 should shift carry into bit 0.
! Returns 1 if working, 0 if broken.
	.globl	test_slal_rl
test_slal_rl:
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
	.globl	test_div_hw
test_div_hw:
	clr	r0
	ld	r1, #42
	div	rr0, #10		! R1=quotient=4, R0=remainder=2
	ld	r0, r1			! return quotient
	ret
