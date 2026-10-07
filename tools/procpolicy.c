/* Actual shared V7 policy with substituted storage and process creation. */
struct user u;
struct proc proc[NPROC];
struct file file[NFILE];
struct inode node;
long testtime;
int failed, corefreed, swapfreed, writes, reads, noswap, ioerr, made;
check(s, ok) char *s;
{ if(!ok) { printf("proc policy: FAIL %s\n",s); failed++; } }
tmalloc(map,n) struct map *map;
{ return(noswap ? 0 : 100); }
tmfree(map,n,a) struct map *map;
{ if(map==coremap)corefreed+=n;else swapfreed+=n; }
corealloc(n) { return(200); }
swapio(b,f,n,r)
{ if(r)reads++;else writes++;return(ioerr ? -1 : 0); }
copyframes() {}
physcopy() {}
readi() {u.u_count=0;}
plock(ip) struct inode *ip; {ip->i_flag |= ILOCK;}
iput(ip) struct inode *ip; {ip->i_count--;ip->i_flag &= ~ILOCK;}
tsleep() {check("unexpected wait",0);}
twake() {}
newproc() {made++;return(0);}
setup()
{
 struct text *xp;
 xp= &text[0];
 xp->x_size=64;xp->x_caddr=200;xp->x_daddr=0;
 xp->x_count=xp->x_ccount=1;xp->x_flag=0;xp->x_iptr= &node;
 node.i_mode=ISVTX;node.i_flag=ITEXT;node.i_count=2;node.i_dev=7;
 corefreed=swapfreed=writes=reads=noswap=ioerr=0;
}
texts()
{
 struct text *xp;
 xp= &text[0];
 setup();textput(xp);
 check("sticky retains inode and swap only",xp->x_count==0 && xp->x_ccount==0 &&
  xp->x_caddr==0 && xp->x_daddr==100 && xp->x_iptr== &node &&
  node.i_count==2 && (node.i_flag&ITEXT) && corefreed==2 && writes==1);
 check("cached text reload",textget(&node,4096)==xp && reads==1 && xp->x_count==1 && xp->x_ccount==1);
 xrele(&node);check("active text cannot release",xp->x_iptr== &node);
 textput(xp);check("immutable backing reused",writes==1 && corefreed==4);
 xumount(8);check("other device retained",xp->x_iptr== &node);
 xumount(7);check("unmount releases cached text",xp->x_iptr==0 && swapfreed==8 && node.i_count==1 && !(node.i_flag&ITEXT));
 xuntext(xp);check("already released entry",node.i_count==1);
 setup();textput(xp);node.i_flag|=ILOCK;xrele(&node);
 check("locked inode release",xp->x_iptr==0 && node.i_count==1 && (node.i_flag&ILOCK));
 setup();noswap=1;textput(xp);
 check("no swap drops cache",xp->x_iptr==0 && corefreed==2 && writes==0);
 setup();ioerr=1;textput(xp);
 check("swap error drops cache",xp->x_iptr==0 && corefreed==2 && swapfreed==8);
 setup();xp->x_flag=XLOAD;textput(xp);
 check("partial load never cached",xp->x_iptr==0 && writes==0);
 setup();xp->x_flag=XTRC;textput(xp);
 check("traced text never cached",xp->x_iptr==0 && writes==0);
 setup();node.i_mode=0;textput(xp);
 check("ordinary text released",xp->x_iptr==0 && writes==0);
 setup();xp->x_count=2;xp->x_ccount=1;textput(xp);
 check("swapped sharer retains backing",xp->x_count==1 && xp->x_ccount==0 && xp->x_daddr==100);
 xp->x_count=0;xrele(&node);
}
forks()
{
 int i;
 for(i=0;i<NPROC;i++){proc[i].p_stat=SRUN;proc[i].p_uid=0;}
 u.u_procp= &proc[1];u.u_uid=1;
 proc[NPROC-1].p_stat=0;made=0;u.u_error=0;forktest();
 check("nonroot final slot reserved",u.u_error==EAGAIN && made==0);
 u.u_uid=0;u.u_error=0;forktest();
 check("root can use final slot",u.u_error==0 && made==1);
 proc[NPROC-1].p_stat=SRUN;u.u_error=0;forktest();
 check("full table root denied",u.u_error==EAGAIN && made==1);
 proc[4].p_stat=0;u.u_uid=1;u.u_error=0;forktest();
 check("nonroot free slot allowed",u.u_error==0 && made==2);
 /* Fixture MAXUPRC=2 makes the original > boundary observable at NPROC=16. */
 for(i=1;i<=2;i++)proc[i].p_uid=1;
 u.u_error=0;forktest();check("V7 limit boundary allowed",u.u_error==0 && made==3);
 proc[3].p_uid=1;u.u_error=0;forktest();
 check("V7 limit exceeded denied",u.u_error==EAGAIN && made==3);
}
main()
{ texts();forks();printf("proc policy: %s\n",failed ? "FAILED" : "passed");return(failed!=0); }
