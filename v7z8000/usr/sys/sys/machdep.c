#include "../h/param.h"
#include "../h/systm.h"
#include "../h/mount.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/proc.h"
#include "../h/inode.h"
#include "../h/buf.h"

/*
 * Machine-dependent code for Z8000 kernel.
 *
 * V7 style with paged MMU (KDSA6 equivalent).
 * No per-process u-area arrays or kernel stacks in BSS.
 * The MMU remaps virtual 0xF000-0xFFFF (u-area + kernel stack)
 * to different physical frames per process.
 */

extern int idle();
extern int outw();

/* Identity frame for copy window (seg1 pages 28-29) */
#define WPAGE_IDENTITY	60

/*
 * Current user segment encoding for cross-segment access.
 * Set by sureg() during context switch. Format: (seg_num << 8) | 0x8000.
 * Used by fubyte/subyte/copyin/copyout in krt.s.
 */
int useg;
int iseg;

/*
 * Segment allocation bitmap.
 * Segments 0 (ROM), 1 (kernel), and 127 (software EPU) are reserved.
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

	for (i = seg_next; i < 127; i++) {
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
	if (segno >= 2 && segno < 127)
		seg_used[segno] = 0;
}

/*
 * Physical frame allocator for u-area pages.
 * Each u-area is 4KB = 2 frames (2KB pages).
 * Frames 0 through (2*NPROC+1)*32-1 are reserved for identity-mapped
 * segments: 0=ROM, 1=kernel, 2..NPROC=user data, and
 * NPROC+2..2*NPROC=user instruction banks. Frames 4064..4095 belong
 * to the reserved EPU service segment and are never allocated here.
 */
static char frame_bmap[512];	/* 4096 frames, 1 bit each */
static int frame_next = (2*NPROC+1)*32;	/* reserve all segment frames */

/*
 * frame_alloc() - allocate a 2-frame pair for a u-area.
 * Returns base frame number, or 0 on failure.
 */
frame_alloc()
{
	register int i;

	for (i = frame_next; i < 127*32; i += 2) {
		if ((frame_bmap[i>>3] & (1 << (i&7))) == 0 &&
		    (frame_bmap[(i+1)>>3] & (1 << ((i+1)&7))) == 0) {
			frame_bmap[i>>3] |= (1 << (i&7));
			frame_bmap[(i+1)>>3] |= (1 << ((i+1)&7));
			frame_next = i + 2;
			return(i);
		}
	}
	/* wrap around */
	for (i = (2*NPROC+1)*32; i < frame_next; i += 2) {
		if ((frame_bmap[i>>3] & (1 << (i&7))) == 0 &&
		    (frame_bmap[(i+1)>>3] & (1 << ((i+1)&7))) == 0) {
			frame_bmap[i>>3] |= (1 << (i&7));
			frame_bmap[(i+1)>>3] |= (1 << ((i+1)&7));
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
	if (f >= (2*NPROC+1)*32 && f < 127*32) {
		frame_bmap[f>>3] &= ~(1 << (f&7));
		frame_bmap[(f+1)>>3] &= ~(1 << ((f+1)&7));
	}
}

/*
 * sureg() - select current process's data and instruction banks.
 * Segment number derived from proc index: proc[0]->seg 1, proc[1]->seg 2, etc.
 */
sureg()
{
	int segno;

	segno = (u.u_procp - proc) + 1;
	useg = (segno << 8) | 0x8000;
	iseg = ((segno + (u.u_sep ? NPROC : 0)) << 8) | 0x8000;
	outw(0x00B8, (segno << 8) | ((iseg >> 8) & 0177));
}

/*
 * clearseg(segno) - zero a 64KB segment.
 * No-op for now: copyout of icode handles initialization.
 */
clearseg(segno)
{
}

/*
 * copyseg(from_seg, to_seg) - copy D space and, when split, I space.
 *
 * Uses the MMU copy window (pages 28-29 at 0xE000-0xEFFF) to access
 * source and destination pages. Copies 2 pages (4KB) per iteration.
 * Identity mapping: segment S page P = frame S*32+P.
 */
copyseg(from_seg, to_seg)
{
	copyspace(from_seg, to_seg);
	if (u.u_sep)
		copyspace(from_seg + NPROC, to_seg + NPROC);
}

copyspace(from_seg, to_seg)
{
	register int i;
	int s, j;
	char buf[16];	/* stack buffer — safe during window remap */

	/*
	 * Keep each remap short enough for clock ticks to be serviced between
	 * chunks. While the window is remapped, BSS in the 0xE000 region is
	 * inaccessible, so restore its identity mapping before splx().
	 */
	for (i = 0; i < 32; i += 2) {
		for (j = 0; j < 4096; j += sizeof(buf)) {
			s = spl7();
			outw(0x00B4, from_seg * 32 + i);
			bcopy(0xE000 + j, buf, sizeof(buf));
			outw(0x00B4, to_seg * 32 + i);
			bcopy(buf, 0xE000 + j, sizeof(buf));
			outw(0x00B4, WPAGE_IDENTITY);
			splx(s);
		}
	}
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

/* plock/prele are now provided by pipe.c with proper sleep/wakeup locking */

/* cinit() is provided by prim.c (clist initialization + device counting) */

/*
 * sbreak -- set break (brk syscall).
 * Grow or shrink data segment.
 * On Z8000, no actual allocation needed (64KB segment already exists).
 * Just validate the new break address.
 */
sbreak()
{
	register unsigned n;

	n = u.u_arg[0];
	if (n == 0) {
		/* brk(0): return current break address */
		u.u_r.r_val1 = ctob(u.u_dsize);
		return;
	}
	/* Ensure break doesn't grow into stack area (leave at least 256 bytes) */
	if (n >= 0xFF00) {
		u.u_error = ENOMEM;
		return;
	}
	u.u_dsize = btoc(n);
}

/*
 * Stubs for functions not needed yet.
 */
startup() {}

/*
 * clkstart() - enable clock interrupts.
 * Called from main() after proc[0] is set up.
 * Enables NVIE in the FCW so clock NVI interrupts are delivered.
 */
clkstart()
{
	spl0();		/* enable VIE+NVIE */
}
xrele(ip) struct inode *ip; {}
xfree() {}
xumount(dev) {}
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
 * icode[] - Z8002 machine code for process 1's initial program.
 *
 * Executes:
 *   exec("/etc/init", 0)  -- sc #11
 *
 * Layout:
 *   0x00: ld R1, #0x000C    (filename pointer)
 *   0x04: ld R2, #0         (argv = NULL)
 *   0x08: sc #11            (exec syscall)
 *   0x0A: halt              (should not reach)
 *   0x0C: "/etc/init\0"
 */
int icode[] = {
	0x2101, 0x000C,		/* ld R1, #0x000C    */
	0x2102, 0x0000,		/* ld R2, #0         */
	0x7F0B,			/* sc #11            */
	0x7A00,			/* halt              */
	/* "/etc/init\0" at offset 0x0C */
	0x2F65, 0x7463, 0x2F69, 0x6E69, 0x7400
};
int szicode = sizeof(icode);
