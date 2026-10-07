/* Parse actual V7-format core files using the target kernel's user layout. */
#include <sys/param.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <stdio.h>
#include <sys/stat.h>
extern long lseek();
extern coreqpc(), corefpc();
int failed;
char marker[512], bytes[512];
struct user saved;
check(name,ok) char *name;
{ if(!ok) {printf("core: FAIL %s\n",name);failed++;} }
getat(fd,off,p,n) long off; char *p;
{ return(lseek(fd,off,0)==off && read(fd,p,n)==n); }
image(mode)
{
 int i,pid,status,fd,gate[2];char stackmark[256],byte;long ds,ss;
 struct stat st;
 unlink("core");
 check("pipe",pipe(gate)==0);
 pid=fork();
 if(!pid) {
  close(gate[0]);
  for(i=0;i<512;i++) marker[i]=i%97;
  for(i=0;i<256;i++) stackmark[i]=i%83;
  write(gate[1],"x",1);close(gate[1]);
  if(mode==0) corequit(getpid());
  if(mode==1) corefault();
  corewait();
  _exit(99);
 }
 close(gate[1]);check("child ready",read(gate[0],&byte,1)==1);close(gate[0]);
 if(mode==2) {sleep(1);kill(pid,SIGQUIT);}
 check("core wait bit",wait(&status)==pid && status==((mode==1?SIGSEG:SIGQUIT)|0200));
 fd=open("core",0);check("core file",fd>=0);if(fd<0)return;
 check("u-area",read(fd,&saved,sizeof(saved))==sizeof(saved));
 ds=(long)saved.u_dsize*64;ss=(long)saved.u_ssize*64;
 check("file size",fstat(fd,&st)==0 && st.st_size==4096L+ds+ss);
 check("creation mode",(st.st_mode&0777)==0644);
 check("data image",getat(fd,4096L+(unsigned)marker,bytes,512));
 for(i=0;i<512;i++) if(bytes[i]!=i%97){check("private data bytes",0);break;}
 check("stack image",getat(fd,4096L+ds+(unsigned)stackmark-(65536L-ss),bytes,256));
 for(i=0;i<256;i++) if(bytes[i]!=i%83){check("stack bytes",0);break;}
 check("general registers",saved.u_regs[4]==0x4444 && saved.u_regs[13]==0x1313 && saved.u_regs[14]==0x1414);
 check("user SP",saved.u_regs[15]>=65536L-ss && saved.u_regs[15]<(unsigned)stackmark);
 check("user FCW",!(saved.u_regs[16]&0xc000));
 if(mode<2) check("saved PC",saved.u_regs[18]==(unsigned)(mode?corefpc:coreqpc));
 check("saved frame pointer",(unsigned)saved.u_ar0>=0xf000 && (unsigned)saved.u_ar0<=0xffde);
 close(fd);
}
pressure()
{
 int gate[2],ready[2],i,n,pid,status,child;char byte;
 unlink("core");pipe(gate);pipe(ready);
 child=fork();
 if(!child) {close(gate[1]);close(ready[0]);write(ready[1],"x",1);read(gate[0],&byte,1);corequit(getpid());_exit(99);}
 close(ready[1]);read(ready[0],&byte,1);close(ready[0]);n=0;
 for(i=0;i<8;i++) {
  pid=fork();if(!pid){close(gate[1]);read(gate[0],&byte,1);_exit(0);}
  if(pid<0){check("pressure fork",0);break;}n++;
 }
 close(gate[0]);
 /* All readers are waiting; release the first (oldest) sleeper, then all. */
 for(i=0;i<=n;i++)write(gate[1],"x",1);close(gate[1]);
 for(i=0;i<=n;i++){pid=wait(&status);check("pressure status",pid>0 && status==(pid==child?(SIGQUIT|0200):0));}
 check("swapped process core",(i=open("core",0))>=0);if(i>=0)close(i);
}
refusals()
{
 int pid,status,fd;struct stat st;
 unlink("core");fd=creat("core",0444);write(fd,"keep",4);close(fd);
 pid=fork();if(!pid){setgid(1);setuid(1);corequit(getpid());_exit(99);}
 check("unwritable core status",wait(&status)==pid && status==SIGQUIT);
 fd=open("core",0);check("unwritable unchanged",read(fd,bytes,4)==4 && bytes[0]=='k');close(fd);
 unlink("core");check("directory fixture",link("directory","core")==0);
 pid=fork();if(!pid){corequit(getpid());_exit(99);}
 check("directory no core flag",wait(&status)==pid && status==SIGQUIT);
 check("directory retained",stat("core",&st)==0 && (st.st_mode&S_IFMT)==S_IFDIR);unlink("core");
 pid=fork();if(!pid){kill(getpid(),SIGTRM);_exit(99);}
 check("ordinary fatal signal",wait(&status)==pid && status==SIGTRM && stat("core",&st)<0);
}
full()
{
 int fd,reserve,pid,status,i;struct stat st;
 unlink("core");fd=creat("core",0666);close(fd);
 reserve=creat("reserve",0600);for(i=0;i<16;i++)write(reserve,bytes,512);close(reserve);
 fd=creat("fill",0600);while(write(fd,bytes,512)==512);close(fd);unlink("reserve");
 pid=fork();if(!pid){corequit(getpid());_exit(99);}
 check("full disk no core bit",wait(&status)==pid && status==SIGQUIT);
 check("partial file",stat("core",&st)==0 && st.st_size>0 && st.st_size<12000L);
 unlink("fill");unlink("core");
 image(0);
}
main(argc,argv) char **argv;
{
 chdir("/tmp");umask(022);
 if(argc>1) full();
 else {image(0);image(1);image(2);refusals();pressure();}
 unlink("core");printf("core: %s\n",failed?"FAILED":"passed");return(failed!=0);
}
