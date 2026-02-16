! =============================================================================
! ROM Init Code for Z8001 (Segment 0)
! Assembled with: z8k-coff-as -z8001 -o rom.o rom.s
!
! Provides the reset vector and initialization code that:
! 1. Sets up the system stack in segment 1
! 2. Configures PSAP to point to the PSA table in segment 1
! 3. Uses IRET to jump to test code in segment 1 in NONSEG+SYS mode
! =============================================================================

	.segm
	.text
	.global	_start

! =============================================================================
! Reset vector at offset 0x0000 (8 bytes)
!
! Format (Z8001):
!   +0x00: reserved (2 bytes)
!   +0x02: FCW (2 bytes)
!   +0x04: segmented PC (4 bytes): high word = (seg<<8)|0x8000, low = offset
! =============================================================================
	.word	0x0000		! reserved
	.word	0xC000		! FCW: SEG + SYS (F_SEG | F_S_N)
	.word	0x8000		! PC high: segment 0, long format
	.word	0x0010		! PC low: offset 0x0010 (init_start)

	! Pad to offset 0x0010 (we have 8 bytes of reset vector, need 8 more)
	.word	0x0000
	.word	0x0000
	.word	0x0000
	.word	0x0000

! =============================================================================
! Init code at offset 0x0010
! Running in SEG+SYS mode, segment 0
! =============================================================================
init_start:
	! Set system stack pointer to segment 1, near top of 64KB
	! In segmented mode, RR14 is the system stack pointer
	! R14 = segment encoding, R15 = offset
	ld	r14, #0x8100	! segment 1 (0x01 << 8 | 0x8000 long format)
	ld	r15, #0xFFF0	! stack offset near top of segment

	! Set PSAP to segment 1, offset 0x0000
	! The PSA table lives at the beginning of the kernel segment
	! PSAPSEG uses encoded segment format: (seg<<8)|0x8000
	ld	r0, #0x8100
	ldctl	psapseg, r0	! PSAP segment = 1 (encoded as 0x8100)
	ld	r0, #0
	ldctl	psapoff, r0	! PSAP offset = 0

	! Set up normal-mode stack pointer (for user mode)
	! Not used in this test, but initialize to something valid
	ld	r0, #0x8200	! segment 2 encoding for normal stack
	ldctl	nspseg, r0
	ld	r0, #0xFFF0
	ldctl	nspoff, r0

	! Now perform a "fake IRET" to jump to test code in segment 1
	! in NONSEG+SYS mode.
	!
	! IRET pops (in segmented mode):
	!   1. tag    = POPW(SP)   - 2 bytes
	!   2. fcw    = POPW(SP)   - 2 bytes
	!   3. pc     = POPL(SP)   - 4 bytes (segmented format)
	!   4. CHANGE_FCW(fcw)
	!
	! Stack grows downward, so we push in reverse order:
	!   First push PC (4 bytes), then FCW (2 bytes), then tag (2 bytes)
	!
	! We want to jump to segment 1, offset 0x0180 (where boot code starts)
	! with FCW = 0x4000 (NONSEG + SYS)

	! Push segmented PC (4 bytes): segment 1, offset 0x0180
	! High word first (push decrements then stores)
	ld	r0, #0x0180	! offset of boot entry in segment 1
	push	@rr14, r0	! push PC low word (offset)
	ld	r0, #0x8100	! segment 1 encoding
	push	@rr14, r0	! push PC high word (segment)

	! Push FCW (2 bytes): NONSEG + SYS = 0x4000
	ld	r0, #0x4000
	push	@rr14, r0

	! Push tag (2 bytes): 0x0000
	ld	r0, #0x0000
	push	@rr14, r0

	! Execute IRET to jump to test code
	! This will:
	!   1. Pop tag (0x0000)
	!   2. Pop FCW (0x4000 = NONSEG+SYS)
	!   3. Pop PC (segment 1, offset 0x0100)
	!   4. CHANGE_FCW(0x4000): SEG->NONSEG transition swaps R14 with m_nspseg
	!      R15 stays (F_S_N unchanged), R14 gets swapped
	iret
