/* Appended to real bio.c, hd.c and binit(), using the target kernel ABI.
 * The controller completes only when finish() is called: read-ahead and
 * write-behind therefore exercise a genuinely outstanding request queue.
 */
char buffers[NBUF][BSIZE];
struct buf buf[NBUF], bfreelist;
struct user u;
int nblkdev;
int failed, commands, completed, sleeping, wakes, level;
int low, high, active, writing, block, status, failblock = -1;
char media[24][BSIZE], transfer[BSIZE];
struct bdevsw bdevsw[] = {
	{ hdopen, hdclose, hdstrategy, &hdtab }, { 0 }
};
check(name,ok) char *name;
{ if(!ok) { printf("bio: FAIL %s\n",name);failed++; } }
panic(s) char *s; { printf("bio: FAIL panic %s\n",s); exit(1); }
spl6() { int old; old=level;level=6;return(old); }
spl0() { int old; old=level;level=0;return(old); }
splx(s) { level=s; }
wakeup(p) char *p; { wakes++; }
finish()
{
	if(!active) { panic("sleep without pending I/O");return; }
	if(block==failblock) status=0x41;
	else {
		status=writing ? 0x40 : 0x48;
		if(writing) bcopy(transfer,media[block],BSIZE);
	}
	active=0;completed++;
	hdintr();
}
sleep(p,pri) char *p;
{ sleeping++;finish();spl0(); }
bcopy(a,b,n) char *a,*b; { while(n--) *b++ = *a++; }
outb(port,value)
{
	if(port==HD_SN) low=value;
	if(port==HD_CL) high=value;
	if(port==HD_CMD) {
		check("controller not overwritten",!active);
		block=low+(high<<8);check("valid block",block>=0 && block<24);
		writing=value==CMD_WRITE;active=1;status=ST_BSY;commands++;
	}
}
inb(port) { return(status); }
outsw(port,p,n) char *p;
{ check("write size",n==256);bcopy(p,transfer,BSIZE); }
insw(port,p,n) char *p;
{ check("read size",n==256);bcopy(media[block],p,BSIZE); }
freecount()
{
	struct buf *bp;int n;
	n=0;
	for(bp=bfreelist.av_forw;bp!= &bfreelist;bp=bp->av_forw) {
		if(++n>NBUF) panic("free list cycle");
		check("free list linkage",bp->av_forw->av_back==bp && !(bp->b_flags&B_BUSY));
	}
	return(n);
}
main()
{
	struct buf *bp,*again;int i,j,before;long hits;
	binit();check("initial buffers",freecount()==NBUF && io_info.nbuf==NBUF);
	for(i=0;i<24;i++) for(j=0;j<BSIZE;j++) media[i][j]=i;
	bp=bread(0,1L);check("read data",bp->b_un.b_addr[0]==1 && sleeping==1);
	brelse(bp);before=commands;hits=io_info.ncache;
	again=bread(0,1L);check("cached read",again==bp && commands==before && io_info.ncache==hits+1 && io_info.bufcount[0]==1);brelse(again);
	bp=getblk(0,2L);bp->b_un.b_addr[0]=42;before=commands;bdwrite(bp);
	check("delayed write",commands==before && media[2][0]==2 && (bp->b_flags&B_DELWRI));
	bp=bread(0,2L);check("dirty cache read",bp->b_un.b_addr[0]==42 && commands==before);brelse(bp);
	bp=getblk(0,3L);bp->b_un.b_addr[0]=43;bdwrite(bp);
	bflush(0);
	check("flush queues both",hdtab.b_actf && hdtab.b_actf->av_forw && commands==before+1);
	while(active) finish();
	check("flush completed",media[2][0]==42 && media[3][0]==43 && freecount()==NBUF);
	bp=breada(0,4L,5L);
	check("read ahead pending",bp->b_un.b_addr[0]==4 && active && io_info.nreada==1);
	brelse(bp);while(active) finish();before=commands;
	bp=bread(0,5L);check("read ahead reused",bp->b_un.b_addr[0]==5 && commands==before);brelse(bp);
	bp=geteblk();bp->b_resid=99;for(i=0;i<BSIZE;i++) bp->b_un.b_addr[i]= -1;
	clrbuf(bp);for(i=0;i<BSIZE;i++) check("clear byte",bp->b_un.b_addr[i]==0);
	check("clear residual",bp->b_resid==0);brelse(bp);
	/* Fill more than the cache: allocation must write dirty victims. */
	for(i=6;i<18;i++) { bp=getblk(0,(long)i);bp->b_un.b_addr[0]=i+50;bdwrite(bp); }
	bflush(NODEV);while(active) finish();
	for(i=6;i<18;i++) check("eviction persistence",media[i][0]==i+50);
	failblock=20;u.u_error=0;bp=bread(0,20L);
	check("read error",(bp->b_flags&B_ERROR) && u.u_error==EIO);
	brelse(bp);check("error invalidates association",!incore(0,20L));
	failblock= -1;u.u_error=0;bp=bread(0,20L);
	check("retry reads again",!u.u_error && bp->b_un.b_addr[0]==20);brelse(bp);
	failblock=21;u.u_error=0;bp=getblk(0,21L);bwrite(bp);
	check("write error",u.u_error==EIO && !incore(0,21L));
	/* Error completion must not strand a following asynchronous request. */
	bp=getblk(0,21L);bawrite(bp);bp=getblk(0,22L);bp->b_un.b_addr[0]=77;bawrite(bp);
	while(active) finish();
	check("queue continues after error",media[22][0]==77 && !incore(0,21L));
	bp=geteblk();bp->b_flags|=B_ERROR;bp->b_error=ENXIO;u.u_error=0;geterror(bp);
	check("specific error",u.u_error==ENXIO);bp->b_error=0;geterror(bp);
	check("default error",u.u_error==EIO);brelse(bp);
	check("final buffers",freecount()==NBUF && !hdtab.b_actf && !hdtab.b_actl && !hdtab.b_active);
	check("completion counts",commands==completed && io_info.nwrite>0 && io_info.nread>0);
	printf("bio: %s\n",failed?"FAILED":"passed");return(failed!=0);
}
