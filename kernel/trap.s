! =============================================================================
! PSA Table + Trap Stubs + Test Code for Z8001 Kernel (Segment 1)
! Assembled with: z8k-coff-as -z8001 -o trap.o trap.s
!
! Assembled in segmented (z8001) mode because the trap handler stubs
! execute in SEG+SYS mode and need segmented register addressing (@RR14).
! The PSA table entries use .word directives with manually-constructed
! segmented address constants.
!
! Memory layout within segment 1 (offset from segment base):
!   0x0000 - 0x003F: PSA table (8 entries x 8 bytes = 64 bytes)
!   0x0040 - 0x00FF: Trap handler stubs
!   0x0100+:         Test code
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
	.word	default_trap	! PC low: offset of default handler

! --- EPU vector (offset 0x08) ---
	.word	0x0000		! reserved
	.word	0xC000		! FCW: SEG + SYS
	.word	0x8100		! PC high: segment 1
	.word	default_trap	! PC low: offset of default handler

! --- TRAP vector (offset 0x10) - privilege violation ---
	.word	0x0000		! reserved
	.word	0xC000		! FCW: SEG + SYS
	.word	0x8100		! PC high: segment 1
	.word	default_trap	! PC low: offset of default handler

! --- SYSCALL vector (offset 0x18) ---
	.word	0x0000		! reserved
	.word	0xC000		! FCW: SEG + SYS
	.word	0x8100		! PC high: segment 1
	.word	syscall_entry	! PC low: offset of syscall handler

! --- SEGTRAP vector (offset 0x20) ---
	.word	0x0000		! reserved
	.word	0xC000		! FCW: SEG + SYS
	.word	0x8100		! PC high: segment 1
	.word	default_trap	! PC low: offset of default handler

! --- NMI vector (offset 0x28) ---
	.word	0x0000		! reserved
	.word	0xC000		! FCW: SEG + SYS
	.word	0x8100		! PC high: segment 1
	.word	default_trap	! PC low: offset of default handler

! --- NVI vector (offset 0x30) ---
	.word	0x0000		! reserved
	.word	0xC000		! FCW: SEG + SYS
	.word	0x8100		! PC high: segment 1
	.word	default_trap	! PC low: offset of default handler

! --- VI vector (offset 0x38) ---
	.word	0x0000		! reserved
	.word	0xC000		! FCW: SEG + SYS
	.word	0x8100		! PC high: segment 1
	.word	default_trap	! PC low: offset of default handler


! =============================================================================
! Trap Handler Stubs (starting at offset 0x0040)
! =============================================================================

! --- Default trap handler: just halt ---
default_trap:
	halt

! =============================================================================
! SYSCALL entry stub
!
! Entry state (set up by CPU hardware):
!   - Mode: SEG + SYS (F_SEG | F_S_N)
!   - RR14 = system stack pointer (segment:offset)
!   - System stack already contains (pushed by CPU):
!       [SP+6,+7]: saved PC high (segmented format)
!       [SP+4,+5]: saved PC low
!       [SP+2,+3]: saved FCW
!       [SP+0,+1]: tag (instruction word, e.g. 0x7F00 for sc #0)
!
! Strategy:
!   1. Save registers R0-R12 onto the system stack
!   2. Switch to NONSEG+SYS to call C handler
!   3. Extract syscall number from tag word, pass args to C handler
!   4. Store C handler return value into saved-R0 slot
!   5. Switch back to SEG+SYS
!   6. Restore registers (R0 gets return value from saved slot)
!   7. IRET to return to caller
! =============================================================================
syscall_entry:
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
	ld	r1, #0x4000	! FCW: NONSEG + SYS
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

	! Write C handler return value (R0) into the saved-R0 slot on
	! the system stack, so it gets restored by pop r0 below.
	ld	0(r15), r0	! saved_regs[0] = return value

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
! Boot entry (at offset 0x0100)
! This runs in NONSEG+SYS mode in segment 1.
! Calls the C handler entry point at 0x0200 which calls main().
! =============================================================================
	.unsegm
	.org	0x0100

_start:
	ld	r2, #0x0202
	call	@r2		! call boot entry at 0x0202 (handler.bin + 2)
	halt

	.segm			! restore segmented mode for object file
