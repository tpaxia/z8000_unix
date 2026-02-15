#include "../h/param.h"
#include "../h/systm.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/proc.h"
#include "../h/inode.h"
#include "../h/conf.h"

/*
 * Machine-dependent code for Z8000 kernel.
 *
 * V7 style with paged MMU (KDSA6 equivalent).
 * No per-process u-area arrays or kernel stacks in BSS.
 * The MMU remaps virtual 0xF000-0xFFFF (u-area + kernel stack)
 * to different physical frames per process.
 */

extern int idle();

/*
 * Current user segment encoding for cross-segment access.
 * Set by sureg() during context switch. Format: (seg_num << 8) | 0x8000.
 * Used by fubyte/subyte/copyin/copyout in krt.s.
 */
int useg;

/*
 * Segment allocation bitmap.
 * Segments 0 (ROM) and 1 (kernel) are reserved.
 * User processes get segments starting from 2.
 */
static char seg_used[128];	/* 128 segments in 8MB */
static int seg_next = 2;	/* next segment to try */

/*
 * seg_alloc() - allocate a segment for a new process.
 * Returns segment number (2+), or 0 on failure.
 */
seg_alloc()
{
	register int i;

	for (i = seg_next; i < 128; i++) {
		if (seg_used[i] == 0) {
			seg_used[i] = 1;
			seg_next = i + 1;
			return(i);
		}
	}
	/* wrap around */
	for (i = 2; i < seg_next; i++) {
		if (seg_used[i] == 0) {
			seg_used[i] = 1;
			seg_next = i + 1;
			return(i);
		}
	}
	return(0);
}

/*
 * seg_free(segno) - free a segment.
 */
seg_free(segno)
{
	if (segno >= 2 && segno < 128)
		seg_used[segno] = 0;
}

/*
 * Physical frame allocator for u-area pages.
 * Each u-area is 4KB = 2 frames (2KB pages).
 * Frames 0-95 are reserved (segments 0-2 identity-mapped).
 */
static char frame_used[4096];	/* 4096 frames in 8MB */
static int frame_next = 96;	/* frames 0-95 reserved */

/*
 * frame_alloc() - allocate a 2-frame pair for a u-area.
 * Returns base frame number, or 0 on failure.
 */
frame_alloc()
{
	register int i;

	for (i = frame_next; i < 4095; i += 2) {
		if (frame_used[i] == 0 && frame_used[i+1] == 0) {
			frame_used[i] = 1;
			frame_used[i+1] = 1;
			frame_next = i + 2;
			return(i);
		}
	}
	/* wrap around */
	for (i = 96; i < frame_next; i += 2) {
		if (frame_used[i] == 0 && frame_used[i+1] == 0) {
			frame_used[i] = 1;
			frame_used[i+1] = 1;
			frame_next = i + 2;
			return(i);
		}
	}
	return(0);
}

/*
 * frame_free(f) - free a 2-frame pair.
 */
frame_free(f)
{
	if (f >= 96 && f < 4096) {
		frame_used[f] = 0;
		frame_used[f+1] = 0;
	}
}

/*
 * sureg() - set useg to current process's segment encoding.
 * Segment number derived from proc index: proc[0]->seg 1, proc[1]->seg 2, etc.
 */
sureg()
{
	int segno;

	segno = (u.u_procp - proc) + 1;
	useg = (segno << 8) | 0x8000;
}

/*
 * clearseg(segno) - zero a 64KB segment.
 * No-op for now: copyout of icode handles initialization.
 */
clearseg(segno)
{
}

/*
 * copyseg(from, to) - copy a click (64 bytes).
 * Not needed (fork copies u-area via MMU window, icode via copyout).
 */
copyseg(from, to)
{
}

/*
 * estabur/expand - no-ops on Z8000 (fixed 64KB segments)
 */
estabur(nt, nd, ns, sep, xrw)
{
	return(0);
}

expand(newsize)
{
}

/*
 * SPL functions: no-ops (no interrupts yet)
 */
spl0() { return(0); }
spl1() { return(0); }
spl4() { return(0); }
spl5() { return(0); }
spl6() { return(0); }
spl7() { return(0); }
splx(s) { return(0); }

/*
 * plock/prele: inode locking (flag-based, no sleep needed yet)
 */
plock(ip)
struct inode *ip;
{
	ip->i_flag |= 01;	/* ILOCK */
}

prele(ip)
struct inode *ip;
{
	ip->i_flag &= ~01;	/* ~ILOCK */
}

/*
 * cinit: count character device switch entries.
 */
cinit()
{
	register struct cdevsw *cdp;

	for (cdp = cdevsw; cdp->d_open; cdp++)
		nchrdev++;
}

/*
 * Stubs for functions not needed yet.
 */
startup() {}
clkstart() {}
xrele(ip) struct inode *ip; {}
xfree() {}
acct() {}

/*
 * bzero: zero count bytes starting at addr.
 */
bzero(addr, count)
char *addr;
int count;
{
	register char *p;
	register int n;

	p = addr;
	n = count;
	while (n--)
		*p++ = 0;
}

/*
 * fuibyte/suibyte/copyiin/copyiout: instruction-space variants.
 * On Z8000 with no I/D separation, these are the same as
 * fubyte/subyte/copyin/copyout (implemented in krt.s).
 */
fuibyte(addr) char *addr; { return(fubyte(addr)); }
suibyte(addr, val) char *addr; { return(subyte(addr, val)); }
copyiin(from, to, n) { return(copyin(from, to, n)); }
copyiout(from, to, n) { return(copyout(from, to, n)); }

/*
 * icode[] - Z8002 machine code for process 1's initial program.
 *
 * Executes:
 *   write(1, msg, 21)    -- sc #4
 *   exit(0)              -- sc #1
 *
 * msg: "hello from process 1\n"
 *
 * Layout:
 *   0x00: ld R1, #1         (fd=stdout)
 *   0x04: ld R2, #0x0018    (buf pointer = offset of msg)
 *   0x08: ld R3, #21        (count)
 *   0x0C: sc #4             (write syscall)
 *   0x0E: ld R1, #0         (exit code)
 *   0x12: sc #1             (exit syscall)
 *   0x14: halt              (should not reach)
 *   0x16: .word 0           (padding)
 *   0x18: "hello from process 1\n\0"
 */
int icode[] = {
	0x2101, 0x0001,		/* ld R1, #1         */
	0x2102, 0x0018,		/* ld R2, #0x0018    */
	0x2103, 0x0015,		/* ld R3, #21        */
	0x7F04,			/* sc #4             */
	0x2101, 0x0000,		/* ld R1, #0         */
	0x7F01,			/* sc #1             */
	0x7A00,			/* halt              */
	0x0000,			/* padding           */
	/* "hello from process 1\n\0" at offset 0x18 */
	0x6865, 0x6C6C, 0x6F20, 0x6672,
	0x6F6D, 0x2070, 0x726F, 0x6365,
	0x7373, 0x2031, 0x0A00
};
int szicode = sizeof(icode);
