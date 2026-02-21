! syscalls.s - System call wrappers for Z8002 user programs.
!
! Convention:
!   Args passed in R1-R5 (loaded from stack).
!   SC #num triggers syscall trap.
!   Return: R0 = result (-1 on error), R1 = errno on error.
!   Wrapper stores errno and returns R0.
!
! Stack layout on entry (no frame pointer):
!   0(R15) = return address
!   2(R15) = arg0
!   4(R15) = arg1
!   6(R15) = arg2
!   8(R15) = arg3
!  10(R15) = arg4

.define _fork, _read, _write, _open, _close
.define _wait, _creat, _link, _unlink, _execve, _chdir
.define _time, _chmod, _chown, _brk, _stat, _lseek
.define _getpid, _setuid, _getuid, _alarm, _fstat, _pause
.define _utime, _stty, _gtty, _access, _nice, _sync
.define _kill, _dup, _pipe, _times, _setgid, _getgid
.define _signal, _ioctl, _umask

.sect .text
.sect .rom
.sect .data
.sect .bss
.sect .text

! ======================================================================
! Common error handler
! R0 = -1, R1 = errno code
! ======================================================================
cerror:
    ld _errno, R1       ! store errno
    ret                 ! return with R0 = -1

! ======================================================================
! int fork(void) -- syscall #2
! Returns: child pid in parent (R0), 0 in child.
! Kernel sets R1=0 (parent) or R1=1 (child).
! ======================================================================
_fork:
    sc $2
    cp R0, $0xFFFF
    jr EQ, cerror
    cp R1, $1           ! child?
    jr NZ, 1f
    ldk R0, $0          ! child returns 0
1:  ret

! ======================================================================
! int read(fd, buf, count) -- syscall #3
! ======================================================================
_read:
    ld R1, 2(R15)       ! fd
    ld R2, 4(R15)       ! buf
    ld R3, 6(R15)       ! count
    sc $3
    cp R0, $0xFFFF
    jr EQ, cerror
    ret

! ======================================================================
! int write(fd, buf, count) -- syscall #4
! ======================================================================
_write:
    ld R1, 2(R15)       ! fd
    ld R2, 4(R15)       ! buf
    ld R3, 6(R15)       ! count
    sc $4
    cp R0, $0xFFFF
    jr EQ, cerror
    ret

! ======================================================================
! int open(name, mode) -- syscall #5
! ======================================================================
_open:
    ld R1, 2(R15)       ! name
    ld R2, 4(R15)       ! mode
    sc $5
    cp R0, $0xFFFF
    jr EQ, cerror
    ret

! ======================================================================
! int close(fd) -- syscall #6
! ======================================================================
_close:
    ld R1, 2(R15)       ! fd
    sc $6
    cp R0, $0xFFFF
    jr EQ, cerror
    ret

! ======================================================================
! int wait(status) -- syscall #7
! Returns child pid; writes exit status to *status if non-NULL.
! Kernel: R0 = child pid, R1 = status (or R0=-1 on error).
! ======================================================================
_wait:
    sc $7
    cp R0, $0xFFFF
    jr EQ, cerror
    ld R2, 2(R15)       ! status pointer
    cp R2, $0
    jr EQ, 1f
    ld *RR2, R1         ! *status = R1 (exit status)
1:  ret

! ======================================================================
! int creat(name, mode) -- syscall #8
! ======================================================================
_creat:
    ld R1, 2(R15)       ! name
    ld R2, 4(R15)       ! mode
    sc $8
    cp R0, $0xFFFF
    jr EQ, cerror
    ret

! ======================================================================
! int link(name1, name2) -- syscall #9
! ======================================================================
_link:
    ld R1, 2(R15)       ! name1
    ld R2, 4(R15)       ! name2
    sc $9
    cp R0, $0xFFFF
    jr EQ, cerror
    ret

! ======================================================================
! int unlink(name) -- syscall #10
! ======================================================================
_unlink:
    ld R1, 2(R15)       ! name
    sc $10
    cp R0, $0xFFFF
    jr EQ, cerror
    ret

! ======================================================================
! int execve(name, argv, envp) -- syscall #11
! ======================================================================
_execve:
    ld R1, 2(R15)       ! name
    ld R2, 4(R15)       ! argv
    ld R3, 6(R15)       ! envp
    sc $11
    ! If we get here, exec failed
    cp R0, $0xFFFF
    jr EQ, cerror
    ret

! ======================================================================
! int chdir(path) -- syscall #12
! ======================================================================
_chdir:
    ld R1, 2(R15)       ! path
    sc $12
    cp R0, $0xFFFF
    jr EQ, cerror
    ret

! ======================================================================
! long time(0) -- syscall #13
! Returns time in R0:R1 (high:low).
! ======================================================================
_time:
    sc $13
    ret

! ======================================================================
! int chmod(name, mode) -- syscall #15
! ======================================================================
_chmod:
    ld R1, 2(R15)       ! name
    ld R2, 4(R15)       ! mode
    sc $15
    cp R0, $0xFFFF
    jr EQ, cerror
    ret

! ======================================================================
! int chown(name, uid, gid) -- syscall #16
! ======================================================================
_chown:
    ld R1, 2(R15)       ! name
    ld R2, 4(R15)       ! uid
    ld R3, 6(R15)       ! gid
    sc $16
    cp R0, $0xFFFF
    jr EQ, cerror
    ret

! ======================================================================
! int brk(addr) -- syscall #17
! ======================================================================
_brk:
    ld R1, 2(R15)       ! addr
    sc $17
    cp R0, $0xFFFF
    jr EQ, cerror
    ret

! ======================================================================
! int stat(name, buf) -- syscall #18
! ======================================================================
_stat:
    ld R1, 2(R15)       ! name
    ld R2, 4(R15)       ! buf
    sc $18
    cp R0, $0xFFFF
    jr EQ, cerror
    ret

! ======================================================================
! off_t lseek(fd, offset, whence) -- syscall #19
! offset is long (2 words on stack).
! Args: R1=fd, R2=off_hi, R3=off_lo, R4=whence.
! Returns off_t in R0:R1.
! ======================================================================
_lseek:
    ld R1, 2(R15)       ! fd
    ld R2, 4(R15)       ! offset high word
    ld R3, 6(R15)       ! offset low word
    ld R4, 8(R15)       ! whence
    sc $19
    cp R0, $0xFFFF
    jr EQ, lseek_err
    ret
lseek_err:
    ld _errno, R1
    ld R1, $0xFFFF      ! return (off_t)-1 = 0xFFFF:0xFFFF
    ret

! ======================================================================
! int getpid(void) -- syscall #20
! ======================================================================
_getpid:
    sc $20
    ret

! ======================================================================
! int setuid(uid) -- syscall #23
! ======================================================================
_setuid:
    ld R1, 2(R15)       ! uid
    sc $23
    cp R0, $0xFFFF
    jr EQ, cerror
    ret

! ======================================================================
! int getuid(void) -- syscall #24
! ======================================================================
_getuid:
    sc $24
    ret

! ======================================================================
! unsigned alarm(secs) -- syscall #27
! ======================================================================
_alarm:
    ld R1, 2(R15)       ! seconds
    sc $27
    ret

! ======================================================================
! int fstat(fd, buf) -- syscall #28
! ======================================================================
_fstat:
    ld R1, 2(R15)       ! fd
    ld R2, 4(R15)       ! buf
    sc $28
    cp R0, $0xFFFF
    jr EQ, cerror
    ret

! ======================================================================
! int pause(void) -- syscall #29
! ======================================================================
_pause:
    sc $29
    ret

! ======================================================================
! int utime(name, times) -- syscall #30
! ======================================================================
_utime:
    ld R1, 2(R15)       ! name
    ld R2, 4(R15)       ! times
    sc $30
    cp R0, $0xFFFF
    jr EQ, cerror
    ret

! ======================================================================
! int stty(fd, buf) -- syscall #31
! ======================================================================
_stty:
    ld R1, 2(R15)       ! fd
    ld R2, 4(R15)       ! buf
    sc $31
    cp R0, $0xFFFF
    jr EQ, cerror
    ret

! ======================================================================
! int gtty(fd, buf) -- syscall #32
! ======================================================================
_gtty:
    ld R1, 2(R15)       ! fd
    ld R2, 4(R15)       ! buf
    sc $32
    cp R0, $0xFFFF
    jr EQ, cerror
    ret

! ======================================================================
! int access(name, mode) -- syscall #33
! ======================================================================
_access:
    ld R1, 2(R15)       ! name
    ld R2, 4(R15)       ! mode
    sc $33
    cp R0, $0xFFFF
    jr EQ, cerror
    ret

! ======================================================================
! int nice(incr) -- syscall #34
! ======================================================================
_nice:
    ld R1, 2(R15)       ! incr
    sc $34
    ret

! ======================================================================
! sync(void) -- syscall #36
! ======================================================================
_sync:
    sc $36
    ret

! ======================================================================
! int kill(pid, sig) -- syscall #37
! ======================================================================
_kill:
    ld R1, 2(R15)       ! pid
    ld R2, 4(R15)       ! sig
    sc $37
    cp R0, $0xFFFF
    jr EQ, cerror
    ret

! ======================================================================
! int dup(fd [, fd2]) -- syscall #41
! Two-arg form: dup(fd|0100, fd2) for dup2.
! ======================================================================
_dup:
    ld R1, 2(R15)       ! fd (possibly OR'd with 0100)
    ld R2, 4(R15)       ! fd2 (for dup2, ignored otherwise)
    sc $41
    cp R0, $0xFFFF
    jr EQ, cerror
    ret

! ======================================================================
! int pipe(fildes) -- syscall #42
! Kernel returns: R0=read_fd, R1=write_fd.
! Wrapper writes to fildes[0] and fildes[1].
! ======================================================================
_pipe:
    sc $42
    cp R0, $0xFFFF
    jr EQ, cerror
    ld R2, 2(R15)       ! fildes pointer
    ld 0(R2), R0        ! fildes[0] = read fd
    ld 2(R2), R1        ! fildes[1] = write fd
    ldk R0, $0          ! return 0 on success
    ret

! ======================================================================
! int times(buf) -- syscall #43
! ======================================================================
_times:
    ld R1, 2(R15)       ! buf
    sc $43
    ret

! ======================================================================
! int setgid(gid) -- syscall #46
! ======================================================================
_setgid:
    ld R1, 2(R15)       ! gid
    sc $46
    cp R0, $0xFFFF
    jr EQ, cerror
    ret

! ======================================================================
! int getgid(void) -- syscall #47
! ======================================================================
_getgid:
    sc $47
    ret

! ======================================================================
! int (*signal(sig, func))() -- syscall #48
! Sets signal handler, returns old handler.
! Kernel: R0 = old handler (or -1 on error).
! ======================================================================
_signal:
    ld R1, 2(R15)       ! sig
    ld R2, 4(R15)       ! func
    sc $48
    cp R0, $0xFFFF
    jr EQ, cerror
    ret

! ======================================================================
! int ioctl(fd, cmd, arg) -- syscall #54
! ======================================================================
_ioctl:
    ld R1, 2(R15)       ! fd
    ld R2, 4(R15)       ! cmd
    ld R3, 6(R15)       ! arg
    sc $54
    cp R0, $0xFFFF
    jr EQ, cerror
    ret

! ======================================================================
! int umask(mask) -- syscall #61
! ======================================================================
_umask:
    ld R1, 2(R15)       ! mask
    sc $61
    ret
