/* Execute the actual sched loop to its next sleep or transfer decision. */
struct proc proc[NPROC];
struct text texts[2];
static jmp_buf stop;
int failed, chosen, victim, inresult, outresult, work, slept;
check(s,ok) char *s;
{if(!ok){printf("sched policy: FAIL %s\n",s);failed++;}}
spl0(){}
spl6(){}
corework(){if(work)longjmp(stop,1);return(0);}
schedsleep(chan,pri) char *chan;
{slept=chan==(char *)&runin ? 1 : chan==(char *)&runout ? 2 : 3;longjmp(stop,1);}
swapin(p) struct proc *p;
{chosen=p-proc;return(inresult);}
swapout(p) struct proc *p;
{victim=p-proc;if(outresult==0)longjmp(stop,1);return(-1);}
reset()
{
 int i;
 for(i=0;i<NPROC;i++){
  proc[i].p_stat=0;proc[i].p_flag=0;proc[i].p_time=0;
  proc[i].p_nice=NZERO;proc[i].p_textp=0;proc[i].p_size=0;
 }
 chosen=victim=slept=runin=runout=work=0;inresult=-1;outresult=0;
}
go(){if(setjmp(stop)==0)sched();}
main()
{
 reset();go();check("idle waits for arrival",slept==2 && runout==1);
 reset();proc[1].p_stat=SRUN;proc[1].p_flag=SLOCK;go();
 check("locked incoming timed retry",slept==1 && chosen==0);
 reset();proc[1].p_stat=SRUN;proc[1].p_textp= &texts[0];texts[0].x_flag=XLOCK;go();
 check("locked incoming text timed retry",slept==1 && chosen==0);
 reset();proc[1].p_stat=proc[2].p_stat=SRUN;proc[1].p_time=10;proc[2].p_time=9;
 proc[2].p_nice=NZERO-1;inresult=0;go();
 check("outage adjusted by nice",chosen==2 && slept==1);
 reset();proc[1].p_stat=proc[2].p_stat=SRUN;proc[1].p_time=2;
 proc[2].p_flag=SLOAD;proc[2].p_time=10;go();
 check("three second outage gate",victim==0 && slept==1);
 proc[1].p_time=3;proc[2].p_time=1;go();
 check("two second residency gate",victim==0 && slept==1);
 proc[2].p_time=2;go();check("aged runnable victim",victim==2);
 reset();proc[1].p_stat=SRUN;
 proc[2].p_stat=SSLEEP;proc[2].p_pri=PZERO;proc[2].p_size=32;proc[2].p_flag=SLOAD;
 proc[3].p_stat=SSTOP;proc[3].p_size=64;proc[3].p_flag=SLOAD;
 go();check("largest sleeper or stopped before age",victim==3);
 reset();proc[1].p_stat=SRUN;proc[1].p_time=127;
 proc[2].p_stat=SRUN;proc[2].p_time=127;proc[2].p_flag=SLOAD|SREADY;
 go();check("first dispatch protected",victim==0 && slept==1);
 proc[2].p_flag=SLOAD;outresult=-1;go();
 check("failed output timed retry",victim==2 && slept==1);
 reset();work=1;go();check("reservations before background work",chosen==0 && slept==0);
 printf("sched policy: %s\n",failed ? "FAILED" : "passed");return(failed!=0);
}
