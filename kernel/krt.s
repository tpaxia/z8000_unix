! Kernel runtime stub (replaces ACK boot.s)
! Entry points called from trap.s in NONSEG+SYS mode.
!
! Layout at 0x0200 (start of handler.bin):
!   0x0200: jr syscall_dispatch   (2 bytes) - SYSCALL handler calls here
!   0x0202: jr boot_entry         (2 bytes) - boot path calls here
!   0x0204: jr nvi_dispatch       (2 bytes) - NVI handler calls here

.define EXIT, WRITE, BRK
.define ERANGE, ESET, EHEAP, EILLINS, EODDZ, ECASE, EBADMON
.define hol0, trppc, trpim, reghp
.define LINO_AD, FILN_AD
.define _putc, _putchar, _inb, _inw, _insw, _outb, _outw, _outsw, _idle
.define _save, _resume, _retu, _set_usp
.define _fubyte, _subyte, _fuword, _suword, _copyin, _copyout
.define _spl0, _spl1, _spl4, _spl5, _spl6, _spl7, _splx

.sect .text
.sect .rom
.sect .data
.sect .bss
.sect .text

LINO_AD = 0
FILN_AD = 4
ERANGE  = 1
ESET    = 2
EHEAP   = 17
EILLINS = 18
EODDZ   = 19
ECASE   = 20
EBADMON = 25

! --- Jump table at offset 0x0000 (address 0x0200) ---
    jr      syscall_dispatch    ! 0x0200: syscall entry
    jr      boot_entry          ! 0x0202: boot entry
    jr      nvi_dispatch        ! 0x0204: NVI handler entry

! --- Syscall dispatch entry ---
! trap.s pushes (num, regs) on the stack, calls 0x0200.
! C-callable wrapper around _trap().
syscall_dispatch:
    push    *SP, R13
    ld      R13, R15
    ld      R0, 4(R13)      ! num
    ld      R1, 6(R13)      ! regs
    sub     R15, $4
    ld      2(R15), R1
    ld      0(R15), R0
    calr    _trap
    add     R15, $4
    ld      R15, R13
    pop     R13, *SP
    ret

! --- NVI dispatch entry ---
! Called from trap.s nvi_entry in NONSEG+SYS mode.
! Calls the C interrupt handler _hdintr().
nvi_dispatch:
    push    *SP, R13
    ld      R13, R15
    calr    _hdintr
    ld      R15, R13
    pop     R13, *SP
    ret

! --- Boot entry ---
boot_entry:
    ld      R2, $begbss
    ld      R3, $endbss
1:  cp      R2, R3
    jr      GE, 2f
    clr     *RR2
    inc     R2, $2
    jr      1b
2:
    ! Kernel stack at top of u-area page (0xF000-0xFFFF).
    ! MMU maps these pages per-process via KDSA6.
    ld      R15, $0xFFFE
    ! Enable NVI: set FCW to NONSEG+SYS+NVIE (0x4800)
    ld      R0, $0x4800
    ldctl   FCW, R0
    calr    _main
    ! After main returns in child process, enter user mode
    calr    _retu
    halt

EXIT:   halt
WRITE:  ret
BRK:    ret

! --- void putc(int ch) ---
_putc:
    push    *SP, R13
    ld      R13, R15
    ld      R1, 4(R13)
    outb    0x00F0, RL1
    ld      R15, R13
    pop     R13, *SP
    ret

! --- void putchar(int ch) ---
_putchar:
    push    *SP, R13
    ld      R13, R15
    ld      R1, 4(R13)
    outb    0x00F0, RL1
    ld      R15, R13
    pop     R13, *SP
    ret

! --- int inb(int port) ---
_inb:
    push    *SP, R13
    ld      R13, R15
    ld      R2, 4(R13)
    inb     RL7, *RR2
    and     R7, $0x00FF
    ld      R0, R7
    ld      R15, R13
    pop     R13, *SP
    ret

! --- void outb(int port, int byte) ---
_outb:
    push    *SP, R13
    ld      R13, R15
    ld      R2, 4(R13)
    ld      R4, 6(R13)
    outb    *RR2, RL4
    ld      R15, R13
    pop     R13, *SP
    ret

! --- void idle(void) ---
_idle:
    halt
    ret

! =============================================================================
! save(label) -- Save context, return 0
! int save(label_t label);
!
! Saves R4-R12, caller's R13, return address, and caller's SP into
! label_t[12].  resume() restores entirely from the label_t without
! depending on the stack contents -- so the u-area copy in newproc()
! can safely clobber save()'s old stack frame.
! =============================================================================
_save:
    push    *SP, R13
    ld      R13, R15
    ld      R1, 4(R13)      ! R1 = label_t pointer (argument)
    ld      0(R1), R4       ! label[0] = R4
    ld      2(R1), R5       ! label[1] = R5
    ld      4(R1), R6       ! label[2] = R6
    ld      6(R1), R7       ! label[3] = R7
    ld      8(R1), R8       ! label[4] = R8
    ld      10(R1), R9      ! label[5] = R9
    ld      12(R1), R10     ! label[6] = R10
    ld      14(R1), R11     ! label[7] = R11
    ld      16(R1), R12     ! label[8] = R12
    ! Save caller's R13 (pushed on stack by our prologue)
    ld      R0, 0(R13)      ! R0 = [R13] = caller's R13
    ld      18(R1), R0      ! label[9] = caller's R13
    ! Save return address (pushed by calr)
    ld      R0, 2(R13)      ! R0 = [R13+2] = return address
    ld      20(R1), R0      ! label[10] = return address
    ! Save caller's SP: R13 + 6 (skip pushed R13 + ret addr + argument)
    ld      R0, R13
    add     R0, $6
    ld      22(R1), R0      ! label[11] = caller's SP
    ldk     R0, $0          ! return 0
    ld      R15, R13
    pop     R13, *SP
    ret

! =============================================================================
! resume(p_addr, label) -- Restore context, return 1
! void resume(int p_addr, label_t label);
!
! Writes KDSA6 (out 0x00B0) to remap the u-area + kernel stack,
! then restores ALL state from label_t (registers, R13, SP, return addr).
! Does NOT depend on stack contents -- stack may have been clobbered
! by bcopy between save() and resume().
!
! No prologue: arguments are read from R15 before the stack is remapped.
! =============================================================================
_resume:
    ld      R0, 2(R15)      ! p_addr = u-area base frame (1st arg)
    ld      R1, 4(R15)      ! label_t pointer (2nd arg, virtual addr in u-area)
    out     0x00B0, R0      ! *** KDSA6: remap u-area pages ***
    ! Now R1 points into the NEW process's label_t (in remapped u-area).
    ld      R4, 0(R1)       ! restore R4-R12
    ld      R5, 2(R1)
    ld      R6, 4(R1)
    ld      R7, 6(R1)
    ld      R8, 8(R1)
    ld      R9, 10(R1)
    ld      R10, 12(R1)
    ld      R11, 14(R1)
    ld      R12, 16(R1)
    ld      R13, 18(R1)     ! restore caller's R13
    ld      R15, 22(R1)     ! restore caller's SP
    ! Push the return address onto the (now correct) stack and return.
    ! This writes 2 bytes below the restored SP -- safe dead zone.
    ld      R2, 20(R1)      ! R2 = return address
    ldk     R0, $1          ! return value = 1
    push    *SP, R2         ! push return address
    ret                     ! pop return address and jump there

! =============================================================================
! Cross-segment memory access functions.
!
! These temporarily switch to SEG+SYS mode for segmented memory access.
! In SEG mode, @RR2 (indirect via register pair R2:R3) gives segmented
! addressing: R2=segment encoding, R3=offset.
!
! The Z8001 CPU decodes register-indirect (IR) mode identically in both
! SEG and NONSEG modes - only base-address (BA/DA) instructions differ.
! So we can assemble in z8002 mode and the IR instructions work in both.
!
! CRITICAL: No BA/DA/X-mode instructions while in SEG mode!
! The CPU would try to decode them with 6-byte segmented format.
! =============================================================================

! --- int fubyte(addr) ---
_fubyte:
    push    *SP, R13
    ld      R13, R15
    ld      R2, _useg       ! user segment encoding (NONSEG, DA ok)
    ld      R3, 4(R13)      ! user offset
    ld      R0, $0xC000
    ldctl   FCW, R0         ! SEG+SYS: R14 swapped
    ! --- SEG mode: only IR/reg/imm instructions ---
    ldb     RL0, *RR2       ! load byte from seg:off
    ld      R1, $0x4000
    ldctl   FCW, R1         ! NONSEG+SYS
    ! --- back to NONSEG mode ---
    and     R0, $0x00FF     ! zero-extend
    ld      R15, R13
    pop     R13, *SP
    ret

! --- int subyte(addr, val) ---
_subyte:
    push    *SP, R13
    ld      R13, R15
    ld      R2, _useg
    ld      R3, 4(R13)      ! user offset
    ld      R4, 6(R13)      ! value
    ld      R0, $0xC000
    ldctl   FCW, R0         ! SEG+SYS
    ldb     *RR2, RL4       ! store byte to seg:off
    ld      R1, $0x4000
    ldctl   FCW, R1         ! NONSEG+SYS
    ldk     R0, $0
    ld      R15, R13
    pop     R13, *SP
    ret

! --- int fuword(addr) ---
_fuword:
    push    *SP, R13
    ld      R13, R15
    ld      R2, _useg
    ld      R3, 4(R13)
    ld      R0, $0xC000
    ldctl   FCW, R0         ! SEG+SYS
    ld      R0, *RR2        ! load word from seg:off
    ld      R1, $0x4000
    ldctl   FCW, R1         ! NONSEG+SYS
    ld      R15, R13
    pop     R13, *SP
    ret

! --- int suword(addr, val) ---
_suword:
    push    *SP, R13
    ld      R13, R15
    ld      R2, _useg
    ld      R3, 4(R13)
    ld      R4, 6(R13)
    ld      R0, $0xC000
    ldctl   FCW, R0         ! SEG+SYS
    ld      *RR2, R4        ! store word to seg:off
    ld      R1, $0x4000
    ldctl   FCW, R1         ! NONSEG+SYS
    ldk     R0, $0
    ld      R15, R13
    pop     R13, *SP
    ret

! =============================================================================
! copyin(from_user, to_kernel, count)
! =============================================================================
_copyin:
    push    *SP, R13
    ld      R13, R15
    ld      R2, _useg       ! user segment encoding
    ld      R3, 4(R13)      ! from: user offset
    ld      R4, 6(R13)      ! to: kernel address
    ld      R5, 8(R13)      ! count
    cp      R5, $0
    jr      EQ, copyin_done
copyin_loop:
    ld      R0, $0xC000
    ldctl   FCW, R0         ! SEG+SYS
    ldb     RL0, *RR2       ! load byte from user space
    ld      R1, $0x4000
    ldctl   FCW, R1         ! NONSEG+SYS
    ldb     *RR4, RL0       ! store byte to kernel (RR4=R4 in z8002)
    inc     R3, $1          ! advance user offset
    inc     R4, $1          ! advance kernel pointer
    dec     R5, $1
    jr      NZ, copyin_loop
copyin_done:
    ldk     R0, $0
    ld      R15, R13
    pop     R13, *SP
    ret

! =============================================================================
! copyout(from_kernel, to_user, count)
! =============================================================================
_copyout:
    push    *SP, R13
    ld      R13, R15
    ld      R2, 4(R13)      ! from: kernel address
    ld      R3, 6(R13)      ! to: user offset
    ld      R5, 8(R13)      ! count
    cp      R5, $0
    jr      EQ, copyout_done
copyout_loop:
    ldb     RL0, *RR2       ! load byte from kernel (RR2=R2 in z8002)
    inc     R2, $1          ! advance kernel pointer
    ld      R6, _useg       ! user segment encoding (NONSEG, DA ok)
    ld      R7, R3          ! user offset
    ld      R4, $0xC000
    ldctl   FCW, R4         ! SEG+SYS
    ldb     *RR6, RL0       ! store byte to user space
    ld      R4, $0x4000
    ldctl   FCW, R4         ! NONSEG+SYS
    inc     R3, $1          ! advance user offset
    dec     R5, $1
    jr      NZ, copyout_loop
copyout_done:
    ldk     R0, $0
    ld      R15, R13
    pop     R13, *SP
    ret

! =============================================================================
! retu() -- Enter user mode.
!
! Builds an IRET frame to transition to NONSEG+NORM in the user segment.
! Must avoid BA/DA mode instructions while in SEG mode.
! =============================================================================
_retu:
    ! Set up registers for the NONSEG+SYS to SEG+SYS transition.
    ! CHANGE_FCW swaps R14 with NSPSEG when the SEG bit changes.
    ! We want R14=0x8100 (kernel seg) after the swap so the system
    ! stack is in the kernel segment.  Put the kernel seg encoding
    ! in NSPSEG (it will be swapped INTO R14) and the user seg
    ! encoding in R14 (it will be swapped INTO NSPSEG for later
    ! use when IRET transitions to user mode).
    ld      R1, _useg       ! user segment encoding (e.g. 0x8200)
    ld      R14, R1         ! R14 = user seg (will go to NSPSEG)
    ld      R0, $0x8100     ! kernel segment encoding
    ldctl   NSPSEG, R0      ! NSPSEG = kernel seg (will go to R14)
    ld      R0, $0xFFF0
    ldctl   NSPOFF, R0      ! user stack offset

    ! Switch to SEG+SYS: R14(useg) swapped with NSPSEG(0x8100)
    ! After: R14=0x8100, NSPSEG=useg, RR14=kernel system stack
    ld      R0, $0xC000
    ldctl   FCW, R0

    ! --- SEG mode: only IR/reg/imm instructions! ---
    ! Build IRET frame on system stack (@RR14):
    !   push PC (4 bytes): user_seg:0x0000
    !   push FCW (2 bytes): 0x0000 (NONSEG+NORM)
    !   push tag (2 bytes): 0x0000
    clr     R0
    push    *RR14, R0       ! PC low: offset 0
    ld      R0, R1          ! R1 = useg (loaded before mode switch)
    push    *RR14, R0       ! PC high: user segment encoding
    clr     R0
    push    *RR14, R0       ! FCW: NONSEG+NORM = 0x0000
    push    *RR14, R0       ! tag: 0x0000

    ! Clear user registers
    clr     R0
    clr     R1
    clr     R2
    clr     R3
    clr     R4
    clr     R5
    clr     R6
    clr     R7
    clr     R8
    clr     R9
    clr     R10
    clr     R11
    clr     R12

    iret                    ! -> NONSEG+NORM, user_seg:0x0000


! --- void set_usp(int value) ---
! Set user stack pointer (NSPOFF control register).
_set_usp:
    push    *SP, R13
    ld      R13, R15
    ld      R0, 4(R13)      ! user SP value
    ldctl   NSPOFF, R0
    ld      R15, R13
    pop     R13, *SP
    ret

! --- int inw(int port) ---
! Word-width I/O input, used for ATA data register.
_inw:
    push    *SP, R13
    ld      R13, R15
    ld      R2, 4(R13)      ! port
    in      R0, *RR2
    ld      R15, R13
    pop     R13, *SP
    ret

! --- void insw(int port, char *addr, int count) ---
! Block word input: read count words from I/O port to memory.
_insw:
    push    *SP, R13
    ld      R13, R15
    ld      R2, 4(R13)      ! port
    ld      R4, 6(R13)      ! addr
    ld      R5, 8(R13)      ! count (words)
    cp      R5, $0
    jr      EQ, insw_done
insw_loop:
    in      R0, *RR2        ! read word from port
    ld      *RR4, R0        ! store to memory
    inc     R4, $2           ! advance pointer by word
    dec     R5, $1
    jr      NZ, insw_loop
insw_done:
    ld      R15, R13
    pop     R13, *SP
    ret

! --- void outsw(int port, char *addr, int count) ---
! Block word output: write count words from memory to I/O port.
_outsw:
    push    *SP, R13
    ld      R13, R15
    ld      R2, 4(R13)      ! port
    ld      R4, 6(R13)      ! addr
    ld      R5, 8(R13)      ! count (words)
    cp      R5, $0
    jr      EQ, outsw_done
outsw_loop:
    ld      R0, *RR4        ! load word from memory
    out     *RR2, R0        ! write word to port
    inc     R4, $2           ! advance pointer by word
    dec     R5, $1
    jr      NZ, outsw_loop
outsw_done:
    ld      R15, R13
    pop     R13, *SP
    ret

! --- void outw(int port, int value) ---
! Word-width I/O output, used for KDSA6 and WPAGE ports.
_outw:
    push    *SP, R13
    ld      R13, R15
    ld      R2, 4(R13)      ! port
    ld      R4, 6(R13)      ! value
    out     *RR2, R4
    ld      R15, R13
    pop     R13, *SP
    ret

! =============================================================================
! SPL functions -- interrupt priority level control.
!
! Z8000 has no priority levels; NVI is simply on or off via NVIE (bit 0x0800).
! spl0/spl1/spl4/spl5: enable interrupts (set NVIE)
! spl6/spl7: disable interrupts (clear NVIE)
! splx(s): restore NVIE from saved FCW value
! All return the previous FCW value (for splx restoration).
! =============================================================================

! --- spl0/spl1/spl4/spl5: enable NVI ---
_spl0:
_spl1:
_spl4:
_spl5:
    ldctl   R0, FCW         ! R0 = old FCW (return value)
    ld      R1, R0
    or      R1, $0x0800     ! set NVIE
    ldctl   FCW, R1
    ret

! --- spl6/spl7: disable NVI ---
_spl6:
_spl7:
    ldctl   R0, FCW         ! R0 = old FCW (return value)
    ld      R1, R0
    and     R1, $0xF7FF     ! clear NVIE
    ldctl   FCW, R1
    ret

! --- splx(s): restore NVIE from argument ---
_splx:
    ldctl   R0, FCW         ! R0 = old FCW (return value)
    ld      R1, R0
    and     R1, $0xF7FF     ! clear NVIE in current
    ld      R2, 2(R15)      ! R2 = argument (saved FCW)
    and     R2, $0x0800     ! isolate NVIE bit
    or      R1, R2          ! copy NVIE from argument
    ldctl   FCW, R1
    ret

.sect .bss
begbss:

.sect .data
hol0:
    .data2 0, 0             ! line number
    .data2 0, 0             ! filename
trppc:
    .data2 0
trpim:
    .data2 0
reghp:
    .data2 endbss
