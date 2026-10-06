#include "../h/param.h"
#include "../h/systm.h"
#include "../h/mount.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/proc.h"
#include "../h/inode.h"
#include "../h/buf.h"
#include "../h/map.h"
#include "../h/conf.h"
#include "../h/text.h"
#include "mmu.h"

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

/* Physical frames are 2KB, not V7's 64-byte accounting clicks. */
#define FIRSTFRAME 96
#define BANKFRAMES 32
#define EPUFRAME (127*32)
#define UFRAMES (USIZE/32)
#define PAGES(n) (((n)+31)/32)
#define DATA 0
#define TEXT 1
#define STACK 2

/* Four extents per process, plus three provisional resize allocations. */
#if CMAPSIZ < 4*(NPROC-1)+5
#error coremap must hold allocation holes, resize reservations and terminator
#endif
#if USIZE != 64
#error paged MMU requires a 4KB u-area and system stack
#endif
#if 2*NPROC >= 127
#error logical user banks must not overlap the EPU segment
#endif

int physmem;
static int corelimit;
struct memspace {
	unsigned base[3];
	unsigned size[3];
};
static struct memspace memory[NPROC];

frame_alloc()
{
	return(corealloc(UFRAMES));
}

frame_free(f)
{
	if (f >= FIRSTFRAME && f <= corelimit-UFRAMES)
		mfree(coremap, UFRAMES, f);
}

/* Each section is a page-sized contiguous extent, as on V7's segmented MMUs.
 * Data grows upward, stack maps at the top, and the intervening gap is absent.
 */
mapspace(slot)
{
	register struct memspace *m;
	int page, d, t, s;
	m = &memory[slot];
	for (page = 0; page < BANKFRAMES; page++) {
		d = t = 0xffff;
		if (page < m->size[DATA])
			d = m->base[DATA] + page;
		if (page >= BANKFRAMES-m->size[STACK])
			d = m->base[STACK] + page - (BANKFRAMES-m->size[STACK]);
		if (page < m->size[TEXT])
			t = m->base[TEXT] + page;
		if (proc[slot].p_textp && page < PAGES(proc[slot].p_textp->x_size))
			t = (proc[slot].p_textp->x_caddr+page) | MM_RO;
		s = spl7();
		outw(0x00BC, (slot+1)*32+page);
		outw(0x00BE, d);
		outw(0x00BC, (slot+1+NPROC)*32+page);
		outw(0x00BE, t);
		splx(s);
	}
	s = spl7();
	outw(MM_STACKSEL, slot+1);
	outw(MM_STACKBASE, m->size[STACK] ?
	    (BANKFRAMES-m->size[STACK])*2048 : 0xffff);
	splx(s);
}

/* Acquire every replacement before changing any old mapping or allocation.
 * Shrinking retains the data prefix or stack suffix. Growing relocates an
 * extent and copies it; failure returns all provisional storage untouched.
 */
resizemem(p, nd, nt, ns)
struct proc *p;
{
	register struct memspace *m;
	unsigned want[3], base[3];
	int i, j, slot;
	slot = p-proc;
	m = &memory[slot];
	if (p->p_textp) nt = 0;
	want[DATA] = nd; want[TEXT] = nt; want[STACK] = ns;
	for (i = 0; i < 3; i++) {
		base[i] = m->base[i];
		if (want[i] > m->size[i]) {
			base[i] = corealloc(want[i]);
			if (!base[i]) {
				for (j = 0; j < i; j++)
					if (want[j] > m->size[j])
						mfree(coremap, want[j], base[j]);
				return(-1);
			}
		}
	}
	for (i = 0; i < 3; i++) {
		if (want[i] > m->size[i]) {
			copyframes(0, base[i], want[i]);
			copyframes(m->base[i], base[i] +
			    (i == STACK ? want[i]-m->size[i] : 0), m->size[i]);
			if (m->size[i])
				mfree(coremap, m->size[i], m->base[i]);
		} else if (want[i] < m->size[i]) {
			mfree(coremap, m->size[i]-want[i], m->base[i] +
			    (i == STACK ? 0 : want[i]));
			if (i == STACK)
				base[i] += m->size[i]-want[i];
		}
		m->base[i] = want[i] ? base[i] : 0;
		m->size[i] = want[i];
	}
	mapspace(slot);
	p->p_size = USIZE + (nd+nt+ns)*32;
	return(0);
}

/* The child gets only its parent's mapped sections, plus its own u-area. */
newmem(p)
struct proc *p;
{
	register struct memspace *m;
	m = &memory[u.u_procp-proc];
	p->p_textp = u.u_procp->p_textp;
	p->p_addr = frame_alloc();
	if (!p->p_addr)
		return(-1);
	if (resizemem(p, m->size[DATA], m->size[TEXT], m->size[STACK]) < 0) {
		frame_free(p->p_addr);
		p->p_addr = p->p_size = 0;
		return(-1);
	}
	if (p->p_textp) {
		p->p_textp->x_count++; p->p_textp->x_ccount++;
	}
	return(0);
}

freemem(p)
struct proc *p;
{
	resizemem(p, 0, 0, 0);
	frame_free(p->p_addr);
	p->p_addr = p->p_size = 0;
}

/* Validate click counts before rounding to hardware pages. Commit size and
 * layout accounting only after every required extent has been acquired.
 * Shared text mappings are read-only; traced writable text (xrw) is unsupported.
 */
estabur(nt, nd, ns, sep, xrw)
unsigned nt, nd, ns;
{
	unsigned dp, tp, sp;
	if (nt > 1024 || nd > 1024 || ns > 1024)
		goto bad;
	tp = sep ? PAGES(nt) : 0;
	dp = PAGES(nd + (sep ? 0 : nt));
	sp = PAGES(ns);
	if (dp+sp > BANKFRAMES ||
	    resizemem(u.u_procp, dp, tp, sp) < 0)
		goto bad;
	u.u_tsize = nt;
	u.u_dsize = nd;
	u.u_ssize = ns;
	u.u_sep = sep;
	sureg();
	return(0);
bad:
	u.u_error = ENOMEM;
	return(-1);
}

/* V7 total-click interface; u-area/text/stack stay fixed while data resizes. */
expand(newsize)
unsigned newsize;
{
	unsigned fixed;
	fixed = USIZE + u.u_tsize + u.u_ssize;
	if (newsize < fixed) {
		u.u_error = ENOMEM;
		return(-1);
	}
	return(estabur(u.u_tsize, newsize-fixed, u.u_ssize, u.u_sep, 0));
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

/* Copy or clear individual physical pages, keeping remaps IRQ-bounded.
 * Only the first page of the two-page window is used: extents can be odd-sized.
 */
copyframes(from, to, pages)
{
	int i, off, s;
	char buf[16];
	for (i = 0; i < pages; i++)
		for (off = 0; off < 2048; off += sizeof(buf)) {
			s = spl7();
			if (from) {
				outw(0x00B4, from+i);
				bcopy(0xE000+off, buf, sizeof(buf));
			} else
				bzero(buf, sizeof(buf));
			outw(0x00B4, to+i);
			bcopy(buf, 0xE000+off, sizeof(buf));
			outw(0x00B4, WPAGE_IDENTITY);
			splx(s);
		}
}

/* plock/prele are now provided by pipe.c with proper sleep/wakeup locking */

/* cinit() is provided by prim.c (clist initialization + device counting) */

/* brk(0) queries the click-rounded break. Real growth can fail with ENOMEM;
 * shrink returns complete pages. New pages are cleared before being mapped.
 */
sbreak()
{
	unsigned n, clicks, old, limit, pos;
	n = u.u_arg[0];
	if (!n) {
		u.u_r.r_val1 = ctob(u.u_dsize + (u.u_sep ? 0 : u.u_tsize));
		return;
	}
	clicks = ((long)n+63) >> 6;
	if (!u.u_sep) {
		if (clicks < u.u_tsize) {
			u.u_error = ENOMEM;
			return;
		}
		clicks -= u.u_tsize;
	}
	old = u.u_dsize + (u.u_sep ? 0 : u.u_tsize);
	if (expand(USIZE + u.u_tsize + clicks + u.u_ssize) < 0)
		return;
	/* The retained last page may hold bytes from an earlier, larger heap.
	 * New physical pages were already cleared by resizemem().
	 */
	limit = PAGES(old)*32;
	clicks = u.u_dsize + (u.u_sep ? 0 : u.u_tsize);
	if (limit > clicks) limit = clicks;
	for (pos = old*64; old < limit && pos < limit*64; pos++)
		subyte(pos, 0);
}

/* Initial u-area mapping installed by the emulated machine at reset. */
mmuinit()
{
	proc[0].p_addr = 62;
	physmem = inw(0x00BA);    /* board-reported installed 2KB frames */
	if (physmem > 4096 || physmem < FIRSTFRAME+UFRAMES+1+PAGES(SSIZE))
		panic("insufficient memory");
	corelimit = physmem < EPUFRAME ? physmem : EPUFRAME;
	mfree(coremap, corelimit-FIRSTFRAME, FIRSTFRAME);
	maxmem = MAXMEM;         /* user I+D ceiling in 64-byte accounting clicks */
}

/* Copy the saved continuation without exposing the remapped window to IRQs. */
copyuarea(p)
struct proc *p;
{
	int s, off;
	for (off = 0; off < 4096; off += 16) {
		s = spl7();
		outw(0x00B4, p->p_addr);
		bcopy(0xF000 + off, 0xE000 + off, 16);
		outw(0x00B4, WPAGE_IDENTITY);
		splx(s);
	}
}

/* Fork copies only mapped pages; fresh init mappings are cleared by estabur. */
copyproc(from, to)
struct proc *from, *to;
{
	register struct memspace *src, *dst;
	int i;
	src = &memory[from-proc]; dst = &memory[to-proc];
	for (i = 0; i < 3; i++)
		copyframes(src->base[i], dst->base[i], src->size[i]);
}

/* Validate every covered data-space page, including gap crossings. */
useracc(base, count, writing)
char *base;
unsigned count;
{
	register struct memspace *m;
	unsigned first, last;
	if ((long)(unsigned)base + count > 65536L)
		return(0);
	if (!count)
		return(1);
	m = &memory[u.u_procp-proc];
	first = (unsigned)base >> 11;
	last = ((long)(unsigned)base+count-1) >> 11;
	for (; first <= last; first++)
		if (first >= m->size[DATA] && first < BANKFRAMES-m->size[STACK])
			return(0);
	return(1);
}

/* Grow from the actual user SP, retaining the old stack at the top. The
 * page allocator checks the data/stack collision before changing anything.
 * A failed speculative growth must not change the interrupted syscall error.
 */
grow(sp)
unsigned sp;
{
	unsigned ns;
	int error, result;
	if (sp == 0)
		return(0);
	ns = (65535-sp)/64+1;
	if (ns <= u.u_ssize)
		return(1);
	if (ns > 1024-SINCR)
		return(0);
	error = u.u_error;
	result = estabur(u.u_tsize, u.u_dsize, ns+SINCR, u.u_sep, 0);
	u.u_error = error;
	return(result == 0);
}

/* Whole-process swapping. The scheduler never resumes a nonresident u-area.
 * Transfers use a private kernel bounce buffer and the ordinary block driver.
 * Wait with interrupts enabled, without sleeping on proc 0's scheduler stack.
 */
static struct buf swbuf;
static char swbounce[512];
int swapouts, swapins;

swapinit()
{
	if (nswap > 1)
		mfree(swapmap, nswap-1, 1);
}

/* Copy a physical extent slice while bounding interrupt masking to 16 bytes. */
physcopy(frame, off, buf, count, writing)
unsigned frame, off, count;
char *buf;
{
	int s, n;
	while (count) {
		n = count > 16 ? 16 : count;
		s = spl7();
		outw(MM_WPAGE, frame+(off>>11));
		if (writing) bcopy(buf, 0xe000+(off&2047), n);
		else bcopy(0xe000+(off&2047), buf, n);
		outw(MM_WPAGE, WPAGE_IDENTITY);
		splx(s);
		buf += n; off += n; count -= n;
	}
}

swapio(block, frame, pages, reading)
unsigned block, frame, pages;
{
	unsigned i;
	int s, error;
	for (i = 0; i < pages*4; i++) {
		if (!reading) physcopy(frame+i/4, (i%4)*512, swbounce, 512, 0);
		swbuf.b_flags = B_BUSY | (reading ? B_READ : B_WRITE);
		swbuf.b_dev = swapdev;
		swbuf.b_blkno = swplo + block+i;
		swbuf.b_bcount = 512;
		swbuf.b_un.b_addr = swbounce;
		swbuf.b_error = swbuf.b_resid = 0;
		s = spl0();
		(*bdevsw[major(swapdev)].d_strategy)(&swbuf);
		while (!(swbuf.b_flags & B_DONE)) idle();
		error = swbuf.b_flags & B_ERROR;
		splx(s);
		if (error) return(-1);
		if (reading) physcopy(frame+i/4, (i%4)*512, swbounce, 512, 1);
	}
	return(0);
}

corealloc(pages)
unsigned pages;
{
	register struct proc *p;
	unsigned f;
	while (!(f = malloc(coremap, pages))) {
		for (p = &proc[1]; p < &proc[NPROC]; p++)
			if (p != u.u_procp && (p->p_flag & (SLOAD|SLOCK|SSYS)) == SLOAD &&
			    (p->p_stat == SSLEEP || p->p_stat == SRUN) && swapout(p) == 0)
				break;
		if (p == &proc[NPROC]) return(0);
	}
	return(f);
}

swapout(p)
struct proc *p;
{
	register struct memspace *m;
	unsigned block, pos, pages;
	int i;
	m = &memory[p-proc];
	pages = UFRAMES;
	for (i = 0; i < 3; i++) pages += m->size[i];
	block = malloc(swapmap, pages*4);
	if (!block) return(-1);
	p->p_flag |= SLOCK;
	if (swapio(block, p->p_addr, UFRAMES, 0) < 0) goto fail;
	pos = block+UFRAMES*4;
	for (i = 0; i < 3; i++) {
		if (swapio(pos, m->base[i], m->size[i], 0) < 0) goto fail;
		pos += m->size[i]*4;
	}
	if (textout(p->p_textp) < 0) goto fail;
	frame_free(p->p_addr);
	for (i = 0; i < 3; i++) {
		if (m->size[i]) mfree(coremap, m->size[i], m->base[i]);
		m->base[i] = 0;
	}
	p->p_addr = block;
	p->p_flag &= ~(SLOAD|SLOCK);
	/* Retain sizes for swapin, but remove every stale physical mapping. */
	for (i = 0; i < 32; i++) {
		outw(MM_PAGESEL, (p-proc+1)*32+i); outw(MM_PAGEFRAME, 0xffff);
		outw(MM_PAGESEL, (p-proc+1+NPROC)*32+i); outw(MM_PAGEFRAME, 0xffff);
	}
	swapouts++;
	return(0);
fail:
	p->p_flag &= ~SLOCK;
	mfree(swapmap, pages*4, block);
	return(-1);
}

swapin(p)
struct proc *p;
{
	register struct memspace *m;
	unsigned frame, base[3], pos, block;
	int i, j;
	if (p->p_flag & SLOAD) return(0);
	p->p_flag |= SLOCK;
	m = &memory[p-proc];
	frame = frame_alloc();
	if (!frame) goto fail;
	for (i = 0; i < 3; i++) {
		base[i] = m->size[i] ? corealloc(m->size[i]) : 0;
		if (m->size[i] && !base[i]) goto release;
	}
	if (textin(p->p_textp) < 0) goto release;
	block = p->p_addr;
	if (swapio(block, frame, UFRAMES, 1) < 0) goto release;
	pos = block+UFRAMES*4;
	for (j = 0; j < 3; j++) {
		if (swapio(pos, base[j], m->size[j], 1) < 0) goto release;
		pos += m->size[j]*4;
	}
	for (j = 0; j < 3; j++) m->base[j] = base[j];
	mfree(swapmap, pos-block, block);
	p->p_addr = frame;
	if (p->p_textp) p->p_textp->x_ccount++;
	p->p_flag = (p->p_flag|SLOAD)&~SLOCK;
	mapspace(p-proc);
	swapins++;
	return(0);
release:
	for (j = 0; j < i; j++)
		if (m->size[j]) mfree(coremap, m->size[j], base[j]);
	frame_free(frame);
fail:
	p->p_flag &= ~SLOCK;
	return(-1);
}

/* V7 fork fallback: write the parent's saved child continuation directly to
 * swap when a second resident copy cannot fit. The parent remains in core.
 */
forkswap(p)
struct proc *p;
{
	register struct memspace *src, *dst;
	unsigned pages, block, pos;
	int i;
	src = &memory[u.u_procp-proc]; dst = &memory[p-proc];
	pages = UFRAMES;
	for (i = 0; i < 3; i++) pages += src->size[i];
	block = malloc(swapmap, pages*4);
	if (!block) return(-1);
	if (swapio(block, u.u_procp->p_addr, UFRAMES, 0) < 0) goto fail;
	pos = block+UFRAMES*4;
	for (i = 0; i < 3; i++) {
		if (swapio(pos, src->base[i], src->size[i], 0) < 0) goto fail;
		pos += src->size[i]*4;
	}
	for (i = 0; i < 3; i++) {
		dst->base[i] = 0; dst->size[i] = src->size[i];
	}
	p->p_addr = block; p->p_size = u.u_procp->p_size;
	p->p_flag &= ~SLOAD;
	if (p->p_textp) p->p_textp->x_count++;
	swapouts++;
	return(0);
fail:
	mfree(swapmap, pages*4, block);
	return(-1);
}
