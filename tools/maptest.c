/* Appended to the actual map allocator and paged allocation helpers. */
int failed, woke, ramframes;
outw(p, v) {}
spl7() { return(0); }
splx(s) {}
copyframes(a, b, n) {}
corealloc(n) { return(malloc(coremap, n)); }
sureg() {}
check(name, ok) char *name;
{ if (!ok) { printf("maps: FAIL %s\n", name); failed++; } }
wakeup(p) char *p;
{ if (p == &runin) woke++; }
inw(port)
{ return(ramframes); }
panic(s) char *s;
{ printf("maps: FAIL panic %s\n", s); exit(1); }
struct proc proc[NPROC];
struct map trial[16];
main()
{
	int a, b, c, i, frames[15];
	mfree(trial, 10, 100);
	mfree(trial, 10, 130);
	a = malloc(trial, 4); b = malloc(trial, 6); c = malloc(trial, 10);
	check("first fit and exact removal", a == 100 && b == 104 && c == 130 && trial[0].m_size == 0);
	mfree(trial, 6, b); mfree(trial, 10, c); mfree(trial, 4, a);
	check("left coalescing", trial[0].m_addr == 100 && trial[0].m_size == 10);
	mfree(trial, 20, 110);
	check("two-sided coalescing", trial[0].m_size == 40 && trial[1].m_size == 0);
	check("oversize preserves map", malloc(trial, 41) == 0 && trial[0].m_size == 40);
	check("full allocation", malloc(trial, 40) == 100 && trial[0].m_size == 0);
	mfree(trial, 10, 110); mfree(trial, 10, 100);
	check("right coalescing", trial[0].m_addr == 100 && trial[0].m_size == 20);
	runin = 1; mfree(trial, 5, 200);
	check("non-core no wakeup", runin == 1 && woke == 0);
	for (i = 0; i < CMAPSIZ; i++) coremap[i].m_size = coremap[i].m_addr = 0;
	ramframes = FIRSTFRAME + 39;
	mmuinit();
	check("sizing and scheduler wakeup", physmem == ramframes && coremap[0].m_addr == FIRSTFRAME &&
	    coremap[0].m_size == 39 && runin == 0 && woke == 1 && proc[0].p_addr == 62);
	malloc(coremap, 32);
	a = frame_alloc(); b = frame_alloc(); c = frame_alloc();
	check("odd RAM tail and exhaustion", a == FIRSTFRAME+32 && b == a+2 && c == b+2 &&
	    frame_alloc() == 0 && coremap[0].m_size == 1);
	frame_free(b); check("reuse hole", frame_alloc() == b);
	frame_free(a); frame_free(c); frame_free(b);
	check("full return", coremap[0].m_addr == FIRSTFRAME+32 && coremap[0].m_size == 7 && coremap[1].m_size == 0);
	for (i = 0; i < CMAPSIZ; i++) coremap[i].m_size = coremap[i].m_addr = 0;
	ramframes = 4096; mmuinit();
	check("EPU excluded", coremap[0].m_size == EPUFRAME-FIRSTFRAME);
	u.u_procp = &proc[1];
	proc[1].p_addr = frame_alloc();
	check("page rounding", estabur(33, 65, 33, 1, 0) == 0 &&
	    memory[1].size[TEXT] == 2 && memory[1].size[DATA] == 3 &&
	    memory[1].size[STACK] == 2 && proc[1].p_size == USIZE+7*32);
	a = memory[1].base[DATA];
	check("data shrink retains prefix", expand(USIZE+33+1+33) == 0 &&
	    memory[1].base[DATA] == a && memory[1].size[DATA] == 1);
	check("stack shrink retains suffix", estabur(33, 1, 1, 1, 0) == 0 &&
	    memory[1].size[STACK] == 1);
	check("page overlap rejected", estabur(0, 993, 32, 0, 0) < 0 && u.u_sep == 1);
	check("oversized text rejected", estabur(1025, 1, 1, 1, 0) < 0);
	c = malloc(coremap, EPUFRAME-(FIRSTFRAME+UFRAMES+7));
	/* There may be holes from shrink: consume each separately. */
	i = 0;
	while ((frames[i] = malloc(coremap, 1)) != 0) i++;
	check("resize failure preserves layout", estabur(96, 96, 64, 0, 0) < 0 &&
	    u.u_sep == 1 && u.u_dsize == 1 && memory[1].base[DATA] == a);
	while (i) mfree(coremap, 1, frames[--i]);
	/* Return the large tail, whose size is known from the initial layout. */
	mfree(coremap, EPUFRAME-(FIRSTFRAME+UFRAMES+7), c);
	freemem(&proc[1]);
	check("all extents returned", coremap[0].m_size == EPUFRAME-FIRSTFRAME);
	/* Failed fork at every partial-allocation boundary must restore the map. */
	proc[1].p_addr = frame_alloc();
	estabur(33, 65, 33, 1, 0);
	for (i = 1; i < UFRAMES+7; i++) {
		c = malloc(coremap, EPUFRAME-FIRSTFRAME-UFRAMES-7-i);
		check("partial fork rollback", newmem(&proc[2]) < 0 &&
		    !proc[2].p_addr && !memory[2].size[DATA] && coremap[0].m_size == i);
		mfree(coremap, EPUFRAME-FIRSTFRAME-UFRAMES-7-i, c);
	}
	freemem(&proc[1]);
	for (i = 0; i < 15; i++) frames[i] = frame_alloc();
	for (i = 0; i < 15; i += 2) frame_free(frames[i]);
	for (i = 1; i < 15; i += 2) frame_free(frames[i]);
	check("fragmented return", coremap[0].m_addr == FIRSTFRAME &&
	    coremap[0].m_size == EPUFRAME-FIRSTFRAME && coremap[1].m_size == 0);
	printf("maps: %s\n", failed ? "FAILED" : "passed");
	return(failed != 0);
}
