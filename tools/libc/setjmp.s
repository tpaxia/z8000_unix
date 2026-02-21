! setjmp.s - setjmp/longjmp for Z8002
!
! jmp_buf layout (8 words = 16 bytes):
!   [0] R8   [1] R9   [2] R10  [3] R11
!   [4] R12  [5] R13  [6] SP   [7] return_addr

.define _setjmp, _longjmp

.sect .text
.sect .rom
.sect .data
.sect .bss
.sect .text

! int setjmp(jmp_buf env)
! Save callee-saved registers, stack pointer, and return address.
! Returns 0.
_setjmp:
    ld R1, 2(R15)       ! R1 = env pointer (first arg)
    ld 0(R1), R8        ! env[0] = R8
    ld 2(R1), R9        ! env[1] = R9
    ld 4(R1), R10       ! env[2] = R10
    ld 6(R1), R11       ! env[3] = R11
    ld 8(R1), R12       ! env[4] = R12
    ld 10(R1), R13      ! env[5] = R13 (frame pointer)
    ! Save caller's SP: after setjmp returns, caller's SP = current SP + 4
    ! (pop return address + caller deallocates 1 arg)
    ld R0, R15
    add R0, $4
    ld 12(R1), R0       ! env[6] = caller's SP
    ld R0, *SP          ! R0 = return address
    ld 14(R1), R0       ! env[7] = return address
    ldk R0, $0          ! return 0
    ret

! void longjmp(jmp_buf env, int val)
! Restore saved state and return val to setjmp callsite.
! If val is 0, returns 1 instead.
_longjmp:
    ld R1, 2(R15)       ! R1 = env pointer
    ld R0, 4(R15)       ! R0 = val
    cp R0, $0
    jr NZ, 1f
    ldk R0, $1          ! val must be nonzero
1:
    ld R8, 0(R1)        ! restore R8
    ld R9, 2(R1)        ! restore R9
    ld R10, 4(R1)       ! restore R10
    ld R11, 6(R1)       ! restore R11
    ld R12, 8(R1)       ! restore R12
    ld R13, 10(R1)      ! restore R13
    ld R15, 12(R1)      ! restore SP (caller's SP)
    ld R2, 14(R1)       ! R2 = return address
    push *SP, R2         ! push return address onto restored stack
    ret                  ! return to setjmp callsite with R0 = val
