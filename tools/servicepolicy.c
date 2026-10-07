/* Target-ABI fixtures for CPU profiling math and actual MMU victim selection. */
struct user u;
struct proc proc[NPROC];
int coremap[2];
int nswap=4096, runin, runout;
spl6(){return(0);}
splx(s){}
coresleep(){check("unexpected allocation sleep",0);}
corewake(){}
reserve()
{
 corereq[NPROC-1].need=1;corereq[NPROC-1].done=0;
 proc[NPROC-1].p_flag=SLOAD|SLOCK;proc[NPROC-1].p_stat=SSLEEP;
 corework();
 check("reservation completion",corereq[NPROC-1].done && !corereq[NPROC-1].need);
 return(corereq[NPROC-1].frame);
}
unsigned memoryword, expectaddr;
int readfail,writefail,failed,available,victim,refuse;
check(s,ok) char *s;
{if(!ok){printf("policy: FAIL %s\n",s);failed++;}}
copyin(addr,p,n) unsigned addr; unsigned *p;
{if(readfail||addr!=expectaddr||n!=2)return(-1);*p=memoryword;return(0);}
copyout(p,addr,n) unsigned *p,addr;
{if(writefail||addr!=expectaddr||n!=2)return(-1);memoryword= *p;return(0);}
coremalloc(map,n) int *map;
{return(available);}
coreswap(p) struct proc *p;
{victim=p-proc;if(victim==refuse)return(-1);available=123;return(0);}
math()
{
 struct { unsigned base, size, off, scale; } p;
 p.base=0x100;p.size=32;p.off=0;p.scale=65535;
 memoryword=0;expectaddr=0x104;addupc(4,&p,1);
 check("full scale",memoryword==1 && p.scale==65535);
 p.scale=32768;expectaddr=0x102;addupc(4,&p,2);check("half scale",memoryword==3);
 p.scale=2;expectaddr=0x102;addupc(0xfffe,&p,1);check("small scale",memoryword==4);
 p.scale=1;addupc(0,&p,1);check("scale one",memoryword==4);
 p.scale=65535;p.off=0x100;addupc(0xfe,&p,1);check("below range",memoryword==4 && p.scale==65535);
 p.off=0;p.size=1;addupc(0,&p,1);check("whole word only",memoryword==4);
 p.size=2;p.scale=2;expectaddr=0x100;memoryword=65535;addupc(0,&p,1);check("counter rollover",memoryword==0 && p.scale==2);
 u.u_error=EINTR;readfail=1;addupc(0,&p,1);check("read fault disables",p.scale==0 && u.u_error==EINTR);readfail=0;
 p.scale=2;writefail=1;addupc(0,&p,1);check("write fault disables",p.scale==0 && u.u_error==EINTR);writefail=0;
 p.scale=65535;p.base=65534;p.size=8;addupc(2,&p,1);check("address wrap disables",p.scale==0);
 p.scale=2;p.base=1;addupc(0,&p,1);check("odd buffer disables",p.scale==0);
}
locking()
{
 int i;
 u.u_procp= &proc[0];
 for(i=1;i<4;i++){proc[i].p_flag=SLOAD|SULOCK;proc[i].p_stat=SRUN;}
 available=0;victim=0;
 check("all locked returns failure",reserve()==0 && victim==0);
 proc[2].p_flag=SLOAD;
 check("unlocked victim",reserve()==123 && victim==2);
 available=0;victim=0;proc[2].p_flag=SLOAD|SLOCK;
 check("kernel lock retained",reserve()==0 && victim==0);
 proc[3].p_flag=SLOAD;proc[3].p_stat=SSTOP;
 check("stopped unlocked victim",reserve()==123 && victim==3);
}
victims()
{
 int i;
 char skip[NPROC];
 struct text tx;
 for(i=0;i<NPROC;i++) {
  proc[i].p_flag=0;proc[i].p_stat=0;skip[i]=0;
  proc[i].p_textp=0;proc[i].p_time=0;proc[i].p_nice=NZERO;
 }
 u.u_procp= &proc[0];
 for(i=1;i<5;i++){proc[i].p_flag=SLOAD;proc[i].p_stat=SRUN;proc[i].p_size=i*32;}
 proc[1].p_time=10;proc[2].p_time=8;proc[2].p_nice=NZERO+3;
 check("resident age plus nice",swapvict(skip)== &proc[2]);
 proc[2].p_flag |= SREADY;
 check("undispatched resident protected",swapvict(skip)== &proc[1]);
 proc[2].p_flag &= ~SREADY;
 proc[3].p_stat=SSLEEP;proc[3].p_pri=PZERO;
 check("sleep preference",swapvict(skip)== &proc[3]);
 proc[4].p_stat=SSTOP;
 check("largest stopped or sleeper",swapvict(skip)== &proc[4]);
 tx.x_flag=XLOCK;proc[4].p_textp= &tx;
 check("locked text excluded",swapvict(skip)== &proc[3]);
 skip[3]=1;
 check("failed transfer excluded",swapvict(skip)== &proc[2]);
 proc[4].p_textp=0;proc[3].p_pri=PZERO-1;
 available=0;victim=0;refuse=4;
 check("failed best victim falls back",reserve()==123 && victim==2);
 refuse=0;
 for(i=1;i<5;i++){proc[i].p_stat=SRUN;proc[i].p_time=0;proc[i].p_nice=0;}
 check("young negative nice still eligible",swapvict(skip)== &proc[1]);
}

main()
{math();locking();victims();printf("policy: %s\n",failed?"FAILED":"passed");return(failed!=0);}
