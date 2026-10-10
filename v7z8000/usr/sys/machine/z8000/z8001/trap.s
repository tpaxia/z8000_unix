! =============================================================================
! PSA Table + Trap Stubs + Test Code for Z8001 Kernel (Segment 1)
! Assembled with shared asz8k -zgs.
!
! Assembled in segmented (z8001) mode because the trap handler stubs
! execute in SEG+SYS mode and need segmented register addressing (@RR14).
! The PSA table entries use .word directives with manually-constructed
! segmented address constants.
!
! Memory layout within segment 1 (offset from segment base):
!   0x0000 - 0x003F: PSA table (8 entries x 8 bytes = 64 bytes)
!   0x0040 - 0x01EF: Trap handler stubs (syscall, NVI clock, VI device)
!   0x01F0:         Boot entry
! =============================================================================

	.segm
	.text
	.global	_start

! =============================================================================
! PSA Table (Program Status Area) at offset 0x0000
!
! Each entry is 8 bytes (Z8001 format):
!   +0: reserved (2 bytes)
!   +2: FCW (2 bytes)
!   +4: segmented PC high word (2 bytes): (seg<<8)|0x8000
!   +6: segmented PC low word (2 bytes): offset
!
! Vector assignments (m_vector_mult=2 for Z8001):
!   Offset 0x00: RST     (PSA + 2*0x00 = PSA + 0x00)  - unused after boot
!   Offset 0x08: EPU     (PSA + 2*0x04 = PSA + 0x08)
!   Offset 0x10: TRAP    (PSA + 2*0x08 = PSA + 0x10)  - privilege violation
!   Offset 0x18: SYSCALL (PSA + 2*0x0C = PSA + 0x18)
!   Offset 0x20: SEGTRAP (PSA + 2*0x10 = PSA + 0x20)
!   Offset 0x28: NMI     (PSA + 2*0x14 = PSA + 0x28)
!   Offset 0x30: NVI     (PSA + 2*0x18 = PSA + 0x30)
!   Offset 0x38: VI      (PSA + 2*0x1C = PSA + 0x38)
! =============================================================================

! --- RST vector (offset 0x00) - unused after boot ---
	.word	0x0000		! reserved
	.word	0xC000		! FCW: SEG + SYS
	.word	0x8100		! PC high: segment 1
	.word	dflttrap	! PC low: offset of default handler

! --- EPU vector (offset 0x08) ---
	.word	0x0000		! reserved
	.word	0xC000		! FCW: SEG + SYS
	.word	0xFF00		! separate software EPU service, segment 127
	.word	0x0000		! fpentry

! --- TRAP vector (offset 0x10) - privilege violation ---
	.word	0x0000		! reserved
	.word	0xC000		! FCW: SEG + SYS
	.word	0x8100		! PC high: segment 1
	.word	dfltpriv	! PC low: offset of priv handler

! --- SYSCALL vector (offset 0x18) ---
	.word	0x0000		! reserved
	.word	0xC000		! FCW: SEG + SYS
	.word	0x8100		! PC high: segment 1
	.word	scentry	! PC low: offset of syscall handler

! --- SEGTRAP vector (offset 0x20) ---
	.word	0x0000		! reserved
	.word	0xC000		! FCW: SEG + SYS
	.word	0x8100		! PC high: segment 1
	.word	0x020A		! PC low: krt.s SEGTRAP entry

! --- NMI vector (offset 0x28) ---
	.word	0x0000		! reserved
	.word	0xC000		! FCW: SEG + SYS
	.word	0x8100		! PC high: segment 1
	.word	dfltnmi	! PC low: offset of NMI handler

! --- NVI vector (offset 0x30) ---
	.word	0x0000		! reserved
	.word	0xC000		! FCW: SEG + SYS (no NVIE — interrupts disabled on entry)
	.word	0x8100		! PC high: segment 1
	.word	nvientry	! PC low: offset of NVI handler

! --- VI vector (offset 0x38) ---
! FCW: SEG + SYS (no VIE/NVIE — interrupts disabled on entry)
! PC field at offset 0x3C doubles as vector 0 entry in the VI vector table
! (VEC00 = PSA + 0x3C, and read_irq_vector reads from VEC00 + 2*vec)
	.word	0x0000		! reserved
	.word	0xC000		! FCW: SEG + SYS
	.word	0x8100		! PC high: segment 1 (= vector 0 PC high)
	.word	vi_entry	! PC low: VI handler (= vector 0 PC low)


! =============================================================================
! Trap Handler Stubs (starting at offset 0x0040)
! =============================================================================

! --- Default trap handlers: halt with distinguishable PCs ---
dflttrap:
	halt
dfltepu:
	halt
dfltpriv:
	! Privilege violation handler: print 'P' + faulting PC, then halt.
	! IRET frame on system stack: tag(+0), FCW(+2), PC_high(+4), PC_low(+6).
	! Switch to NONSEG+SYS so we can use BA-mode addressing on R15.
	ld	r1, #0x4000
	ldctl	fcw, r1		! NONSEG + SYS
	.unsegm
	ld	r0, #0x00F0	! console port
	ld	r1, #0x0050	! 'P'
	outb	@r0, rl1
	ld	r2, 4(r15)	! PC high (segment)
	calr	.Lhex4
	ld	r1, #0x002E	! '.'
	outb	@r0, rl1
	ld	r2, 6(r15)	! PC low (offset)
	calr	.Lhex4
	ld	r1, #0x003A	! ':'
	outb	@r0, rl1
	ld	r2, 0(r15)	! tag (faulting opcode)
	calr	.Lhex4
	ld	r1, #0x002F	! '/'
	outb	@r0, rl1
	ld	r2, 2(r15)	! FCW
	calr	.Lhex4
	halt

! Print r2 as 4 hex digits via port @r0. Clobbers r1,r2,r3.
.Lhex4:
	ld	r3, r2
	srl	r2, #4
	srl	r2, #4
	srl	r2, #4
	calr	.Lhexnib
	ld	r2, r3
	srl	r2, #4
	srl	r2, #4
	calr	.Lhexnib
	ld	r2, r3
	srl	r2, #4
	calr	.Lhexnib
	ld	r2, r3
	calr	.Lhexnib
	ret

! Print low nibble of r2 as hex char via port @r0. Clobbers r1.
.Lhexnib:
	ld	r1, r2
	and	r1, #0x000F
	add	r1, #0x0030	! + '0'
	cp	r1, #0x003A
	jr	lt, .Lhn1
	add	r1, #0x0007	! A-F offset
.Lhn1:
	outb	@r0, rl1
	ret

	.segm
dfltnmi:
	halt

! =============================================================================
! SYSCALL entry stub
!
! Entry state (set up by CPU hardware):
!   - Mode: SEG + SYS (F_SEG | F_S_N)
!   - RR14 = system stack pointer (segment:offset)
!   - System stack already contains (pushed by CPU):
!       [SP+6,+7]: saved PC low
!       [SP+4,+5]: saved PC high (segmented format)
!       [SP+2,+3]: saved FCW
!       [SP+0,+1]: tag (instruction word, e.g. 0x7F00 for sc #0)
!
! Strategy:
!   1. Save registers R0-R12 onto the system stack
!   2. Switch to NONSEG+SYS with interrupts enabled to call C handler
!   3. Extract syscall number from tag word, pass args to C handler
!   4. C handler writes saved R0/R1 and handles signals/rescheduling
!   5. Switch back to SEG+SYS
!   6. Restore registers (R0 gets return value from saved slot)
!   7. IRET to return to caller
! =============================================================================
scentry:
	! Save registers R0-R12 onto system stack (via @RR14 in seg mode)
	! Push in reverse order so R0 is at top of saved area
	push	@rr14, r12
	push	@rr14, r11
	push	@rr14, r10
	push	@rr14, r9
	push	@rr14, r8
	push	@rr14, r7
	push	@rr14, r6
	push	@rr14, r5
	push	@rr14, r4
	push	@rr14, r3
	push	@rr14, r2
	push	@rr14, r1
	push	@rr14, r0

	! Switch to NONSEG+SYS mode for C handler call.
	! Changing F_SEG within system mode causes the CPU to swap R14
	! with the saved system stack segment register, preserving the
	! stack segment for later restoration. R15 (stack offset) is
	! unchanged since F_S_N stays set.
	ld	r1, #0x5800	! FCW: NONSEG + SYS + VIE + NVIE
	ldctl	fcw, r1

	! --- Now in NONSEG+SYS mode, segment 1 (inherited from PC) ---
	! R15 = system stack offset. Saved registers at R15+0..R15+24,
	! tag word at R15+26.
	!
	! Switch assembler to nonseg mode so BA-mode instructions
	! (e.g., 26(r15)) get 4-byte z8002 encodings instead of
	! 6-byte z8001 X-mode encodings.
	.unsegm

	! Extract syscall number from SC instruction tag word
	ld	r0, 26(r15)	! r0 = tag word
	and	r0, #0xFF	! r0 = syscall number

	! Set up arguments for C handler:
	!   arg1 = syscall number (r0)
	!   arg2 = pointer to saved registers (r15)
	ld	r1, r15		! r1 = &saved_regs[0] (before SP adjustment)
	sub	r15, #4		! allocate space for 2 arguments
	ld	2(r15), r1	! [SP+2] = arg2 (regs pointer)
	ld	0(r15), r0	! [SP+0] = arg1 (syscall number)

	! Call C handler entry point at 0x0200
	ld	r2, #0x0200
	call	@r2		! pushes 2-byte ret addr (NONSEG mode)

	! Clean up pushed arguments (2 words = 4 bytes)
	add	r15, #4

	! NOTE: Do NOT overwrite saved-R0 here.
	! The C trap handler (trap.c) already wrote the correct return
	! values into regs[0] and regs[1] on the stack.

	! Switch assembler back to segmented mode
	.segm

	! Switch back to SEG+SYS mode for register restore and IRET.
	! The CPU swaps R14 with the saved stack segment again, restoring
	! RR14 as a valid segmented stack pointer.
	ld	r1, #0xC000	! FCW: SEG + SYS
	ldctl	fcw, r1

	! Restore saved registers (R0 now holds return value from C handler)
	pop	r0, @rr14
	pop	r1, @rr14
	pop	r2, @rr14
	pop	r3, @rr14
	pop	r4, @rr14
	pop	r5, @rr14
	pop	r6, @rr14
	pop	r7, @rr14
	pop	r8, @rr14
	pop	r9, @rr14
	pop	r10, @rr14
	pop	r11, @rr14
	pop	r12, @rr14

	! R0 already has the syscall return value (written to saved slot above)
	iret


! =============================================================================
! NVI (Non-Vectored Interrupt) entry stub — Clock interrupt
!
! Entry state (set up by CPU hardware):
!   - Mode: SEG + SYS (F_SEG | F_S_N), NVIE cleared (interrupts disabled)
!   - RR14 = system stack pointer (segment:offset)
!   - System stack already contains (pushed by CPU):
!       [SP+6,+7]: saved PC low
!       [SP+4,+5]: saved PC high (segmented format)
!       [SP+2,+3]: saved FCW (has NVIE set — will be restored by IRET)
!       [SP+0,+1]: tag (interrupt vector, 0x0000 for NVI)
!
! Strategy:
!   1. Read interrupted FCW from IRET frame (needed by clock())
!   2. Save registers R0-R12 onto the system stack
!   3. Switch to NONSEG+SYS, pass FCW in R0 to nvi_dispatch
!   4. Dispatch handles user-return work, then masks and returns to SEG+SYS
!   5. Restore registers
!   6. IRET to return (restores FCW with NVIE set)
! =============================================================================
nvientry:
	! Save registers R0-R12 onto system stack (via @RR14 in seg mode)
	push	@rr14, r12
	push	@rr14, r11
	push	@rr14, r10
	push	@rr14, r9
	push	@rr14, r8
	push	@rr14, r7
	push	@rr14, r6
	push	@rr14, r5
	push	@rr14, r4
	push	@rr14, r3
	push	@rr14, r2
	push	@rr14, r1
	push	@rr14, r0

	! Switch to NONSEG+SYS mode for C handler call
	ld	r1, #0x4000	! FCW: NONSEG + SYS (no NVIE)
	ldctl	fcw, r1

	.unsegm

	! Stack layout (NONSEG mode):
	!   R15+0:  saved R0
	!   R15+26: tag word
	!   R15+28: saved FCW (interrupted process's FCW)
	! Read interrupted FCW and pass to clock() via nvi_dispatch
	ld	r0, 28(r15)	! R0 = interrupted FCW

	! Call NVI dispatch at 0x0204
	ld	r2, #0x0204
	call	@r2

	.segm

	! Switch back to SEG+SYS mode for register restore and IRET
	ld	r1, #0xC000	! FCW: SEG + SYS
	ldctl	fcw, r1

	! Restore saved registers
	pop	r0, @rr14
	pop	r1, @rr14
	pop	r2, @rr14
	pop	r3, @rr14
	pop	r4, @rr14
	pop	r5, @rr14
	pop	r6, @rr14
	pop	r7, @rr14
	pop	r8, @rr14
	pop	r9, @rr14
	pop	r10, @rr14
	pop	r11, @rr14
	pop	r12, @rr14

	! IRET restores FCW (which has NVIE set), re-enabling interrupts
	iret


! =============================================================================
! VI (Vectored Interrupt) entry stub — Device interrupts
!
! Entry state (set up by CPU hardware):
!   - Mode: SEG + SYS (F_SEG | F_S_N), VIE cleared
!   - RR14 = system stack pointer (segment:offset)
!   - System stack already contains (pushed by CPU):
!       [SP+6,+7]: saved PC low
!       [SP+4,+5]: saved PC high (segmented format)
!       [SP+2,+3]: saved FCW
!       [SP+0,+1]: tag (vector identifier)
!
! Note: The CPU reads the new PC from the vector table (VEC00 + 2*vec),
! NOT from the PSA VI entry. For vector 0, VEC00 = PSA + 0x3C, which
! overlaps with the PC field of the PSA VI entry. So for vector 0,
! the CPU jumps here (vi_entry) via the vector table.
!
! The vector identifier is pushed as the tag word. We need to extract
! it from the IRET frame on the stack and pass it to vi_dispatch.
!
! Stack layout after CPU push (before our saves):
!   @RR14 → tag           (+0)
!            saved FCW    (+2)
!            saved PC_high (+4)
!            saved PC_low (+6)
! CPU push order: PC(4), FCW(2), tag(2).
! =============================================================================
vi_entry:
	! Save registers R0-R12 onto system stack (via @RR14 in seg mode)
	push	@rr14, r12
	push	@rr14, r11
	push	@rr14, r10
	push	@rr14, r9
	push	@rr14, r8
	push	@rr14, r7
	push	@rr14, r6
	push	@rr14, r5
	push	@rr14, r4
	push	@rr14, r3
	push	@rr14, r2
	push	@rr14, r1
	push	@rr14, r0

	! Switch to NONSEG+SYS mode
	ld	r1, #0x4000	! Keep both interrupt classes masked during the mode switch
	ldctl	fcw, r1

	.unsegm
	! Admit clock nesting only if the interrupted context had enabled it.
	ld	r1, 28(r15)
	and	r1, #0x0800
	or	r1, #0x4000
	ldctl	fcw, r1


	! Stack layout (NONSEG mode):
	!   R15+0:  saved R0
	!   R15+26: tag word (vector identifier)
	! Read vector identifier and pass to vi_dispatch in R0
	ld	r0, 26(r15)	! R0 = vector identifier

	! Call VI dispatch at 0x0206
	ld	r2, #0x0206
	call	@r2

	.segm

	! Switch back to SEG+SYS mode
	ld	r1, #0xC000	! FCW: SEG + SYS
	ldctl	fcw, r1

	! Restore saved registers
	pop	r0, @rr14
	pop	r1, @rr14
	pop	r2, @rr14
	pop	r3, @rr14
	pop	r4, @rr14
	pop	r5, @rr14
	pop	r6, @rr14
	pop	r7, @rr14
	pop	r8, @rr14
	pop	r9, @rr14
	pop	r10, @rr14
	pop	r11, @rr14
	pop	r12, @rr14

	iret


! =============================================================================
! Boot entry (at offset 0x01F0)
! This runs in NONSEG+SYS mode in segment 1.
! Calls the C handler entry point at 0x0200 which calls main().
! =============================================================================
	.unsegm
	.org	0x01F0

_start:
	ld	r2, #0x0202
	call	@r2		! call boot entry at 0x0202 (handler.bin + 2)
	halt

	.segm			! restore segmented mode for object file
