#include <sys/param.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <stdio.h>
#include <traceaddr.h>
extern int errno;
extern tracego(),traceval();
struct user layout;
int marker= -1, failed, regoff;
check(s,ok) char *s;
{ if(!ok){printf("trace: FAIL %s errno=%d\n",s,errno);failed++;} }
request(req,pid,addr,data) unsigned addr;
{ return(ptrace(req,pid,(int *)addr,data)); }
readreg(pid,r) { return(request(3,pid,regoff+2*r,0)); }
putreg(pid,r,v) { check("write register",request(6,pid,regoff+2*r,v)!=-1 || !errno); }
basic()
{
 int pid,status,helper,sp,fcw,split,pc;
 pid=fork();
 if(!pid) {
  ptrace(0,0,0,0);signal(SIGTRC,1);
  if(tracerun(getpid()) || marker!=0x1234) _exit(71);
  _exit(0);
 }
 check("signal stop",wait(&status)==pid && status==((SIGTRC<<8)|0177));
 helper=fork();
 if(!helper) _exit(!(request(2,pid,(unsigned)&marker,0)==-1 && errno==ESRCH));
 check("only parent can trace",wait(&status)==helper && status==0);
 check("read minus one",request(2,pid,(unsigned)&marker,0)==-1 && errno==0);
 check("write data",request(5,pid,(unsigned)&marker,0x1234)==0x1234);
 check("read changed data",request(2,pid,(unsigned)&marker,0)==0x1234);
 split=request(3,pid,(char *)&layout.u_sep-(char *)&layout,0);
 check("read instruction",request(1,pid,(unsigned)traceval+2,0)==7);
 if(split) check("shared text refused",request(4,pid,(unsigned)traceval+2,9)==-1 && errno==EIO);
 else check("combined text write",request(4,pid,(unsigned)traceval+2,9)==9 && request(1,pid,(unsigned)traceval+2,0)==9);
 check("parent text unchanged",traceval()==7);
 check("odd address",request(2,pid,1,0)==-1 && errno==EIO);
 check("gap address",request(5,pid,0x8000,0)==-1 && errno==EIO);
 check("wrapping address",request(1,pid,0xffff,0)==-1 && errno==EIO);
 check("u-area bounds",request(3,pid,4096,0)==-1 && errno==EIO);
 check("credentials readonly",request(6,pid,(char *)&layout.u_uid-(char *)&layout,0)==-1 && errno==EIO);
 check("segment readonly",request(6,pid,regoff+34,0)==-1 && errno==EIO);
 check("odd PC rejected",request(6,pid,regoff+36,3)==-1 && errno==EIO);
 pc=readreg(pid,18);
 check("hardware step unavailable",request(9,pid,1,0)==-1 && errno==EIO && readreg(pid,18)==pc);
 check("bad signal rejected",request(7,pid,1,NSIG)==-1 && errno==EIO);
 check("bad request",request(99,pid,1,0)==-1 && errno==EIO);
 fcw=readreg(pid,16);putreg(pid,16,0xffff);
 check("FCW privilege preserved",readreg(pid,16)==((fcw&~0xfc)|0xfc));
 sp=readreg(pid,15);putreg(pid,15,sp-2);
 putreg(pid,4,0x4567);putreg(pid,13,0x1357);putreg(pid,14,0x2468);
 check("continue with PC",request(7,pid,(unsigned)tracego,0)==0);
 check("registers applied",wait(&status)==pid && status==0);
 check("dead child rejected",request(2,pid,0,0)==-1 && errno==ESRCH);
}
lifecycle()
{
 int pid,status;
 pid=fork();if(!pid){ptrace(0,0,0,0);kill(getpid(),SIGTRC);_exit(77);}
 check("second stop",wait(&status)==pid && (status&255)==0177);
 check("deliver signal",request(7,pid,1,SIGTRM)==SIGTRM);
 check("signal termination",wait(&status)==pid && status==SIGTRM);
 pid=fork();if(!pid){ptrace(0,0,0,0);kill(getpid(),SIGTRC);_exit(77);}
 check("kill stop",wait(&status)==pid && (status&255)==0177);
 check("force exit",request(8,pid,0,0)==0);
 check("forced status",wait(&status)==pid && status==SIGTRC);
}
main()
{
 regoff=(char *)&layout.u_regs-(char *)&layout;
 basic();lifecycle();execs();orphans();concurrent();
 printf("trace: %s\n",failed?"FAILED":"passed");return(failed!=0);
}

pressure(child,addr,expected)
unsigned addr;
{
 int gate[2],i,n,pid,status;char byte;
 pipe(gate);n=0;
 for(i=0;i<8;i++) {
  pid=fork();if(!pid){close(gate[1]);read(gate[0],&byte,1);_exit(0);}
  if(pid<0){check("pressure fork",0);break;}n++;
 }
 check("stopped image after pressure",request(1,child,addr,0)==expected);
 check("restop after inspection",wait(&status)==child && (status&255)==0177);
 close(gate[0]);for(i=0;i<n;i++)write(gate[1],"x",1);close(gate[1]);
 for(i=0;i<n;i++)check("pressure reap",wait(&status)>0 && status==0);
}
execs()
{
 int k,pid,status,helper,opcode;char *path;
 for(k=0;k<2;k++) {
  path=k?"/bin/targeti":"/bin/targetn";
  pid=fork();if(!pid){ptrace(0,0,0,0);execl(path,path,0);_exit(99);}
  check("exec trap",wait(&status)==pid && status==((SIGTRC<<8)|0177));
  check("exec data",request(2,pid,targetdata[k],0)==-1 && !errno);
  pressure(pid,targetword[k],7);
  check("exec data patch",request(5,pid,targetdata[k],0x1234)==0x1234);
  check("exclusive instruction patch",request(4,pid,targetword[k],9)==9);
  pressure(pid,targetword[k],9);
  if(k) {
   helper=fork();if(!helper){execl(path,path,"clean",0);_exit(errno==ETXTBSY?0:99);}
   check("patched text cannot be shared by exec",wait(&status)==helper && status==0);
  }
  opcode=request(1,pid,targetword[k]-2,0);
  check("insert breakpoint",request(4,pid,targetword[k]-2,0x7fff)==0x7fff);
  check("continue to breakpoint",request(7,pid,1,0)==0);
  check("breakpoint stop",wait(&status)==pid && status==((SIGTRC<<8)|0177));
  check("breakpoint PC",readreg(pid,18)==targetword[k]);
  check("restore instruction",request(4,pid,targetword[k]-2,opcode)==opcode);
  check("continue patched image",request(7,pid,targetword[k]-2,0)==0);
  check("patched execution",wait(&status)==pid && status==0);
  helper=fork();if(!helper){execl(path,path,"clean",0);_exit(99);}
  check("disk text unchanged and released",wait(&status)==helper && status==0);
 }
 chmod("/bin/targeti",01755);
 pid=fork();if(!pid){ptrace(0,0,0,0);execl("/bin/targeti","targeti",0);_exit(99);}
 check("sticky exec stop",wait(&status)==pid && (status&255)==0177);
 check("sticky text refused",request(4,pid,targetword[1],9)==-1 && errno==EIO);
 request(8,pid,0,0);check("sticky child reaped",wait(&status)==pid && status==SIGTRC);
 chmod("/bin/targeti",0755);
}
orphans()
{
 int pipefd[2],parent,child,status;char byte;
 pipe(pipefd);parent=fork();
 if(!parent) {
  close(pipefd[0]);child=fork();
  if(!child){ptrace(0,0,0,0);kill(getpid(),SIGTRC);_exit(99);}
  if(wait(&status)!=child || (status&255)!=0177)_exit(1);
  _exit(0);
 }
 close(pipefd[1]);check("orphan debugger exit",wait(&status)==parent && status==0);
 check("orphan tracee released descriptors",read(pipefd[0],&byte,1)==0);close(pipefd[0]);
}

concurrent()
{
 int helper,child,i,j,status;
 for(i=0;i<2;i++) {
  helper=fork();
  if(!helper) {
   child=fork();
   if(!child){ptrace(0,0,0,0);kill(getpid(),SIGTRC);_exit(99);}
   check("parallel stop",wait(&status)==child && (status&255)==0177);
   for(j=0;j<20;j++) {
    check("parallel write",request(5,child,(unsigned)&marker,j+i*100)==j+i*100);
    check("parallel read",request(2,child,(unsigned)&marker,0)==j+i*100);
   }
   request(8,child,0,0);check("parallel reap",wait(&status)==child && status==SIGTRC);
   _exit(failed!=0);
  }
 }
 for(i=0;i<2;i++) check("parallel debuggers",wait(&status)>0 && status==0);
}
