#include <sys/param.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/acct.h>
#include <sys/stat.h>
#include <sys/reg.h>
#include <setjmp.h>
#include <stdio.h>
extern long time(), lseek();
extern int errno;
int failed, noise;
unsigned samples[4];
check(s,ok) char *s;
{ if(!ok) {printf("services: FAIL %s errno=%d\n",s,errno);failed++;} }
burn()
{ int i,j;for(i=0;i<80;i++)for(j=0;j<2000;j++)noise+=j; }
profile()
{
 int pid,status;unsigned old;
 samples[0]=0;profil(samples,2,0,2);burn();profil(0,0,0,0);
 check("clock samples",samples[0]>0 && samples[1]==0);
 old=samples[0];burn();check("profil disabled",samples[0]==old);
 profil(samples,2,0,1);burn();profil(0,0,0,0);
 check("scale one disabled",samples[0]==old);
 profil(samples,0,0,2);burn();profil(0,0,0,0);
 check("empty buffer",samples[0]==old);
 /* Bad buffers disable collection without killing the process. */
 profil((char *)1,2,0,2);burn();profil(0,0,0,0);
 profil((char *)0x8000,2,0,2);burn();profil(0,0,0,0);
 samples[0]=0;profil(samples,2,0,2);
 pid=fork();
 if(!pid) {old=samples[0];burn();profil(0,0,0,0);_exit(samples[0]<=old);}
 check("fork retains profiling",pid>0 && wait(&status)==pid && status==0);
 profil(0,0,0,0);
 pid=fork();
 if(!pid) {profil(samples,2,0,2);execl("/bin/services","services","profexec",0);_exit(90);}
 check("exec stops profiling",pid>0 && wait(&status)==pid && status==0);
}
histtest()
{
 unsigned words[64];int i,fd,n;long total;
 for(i=0;i<64;i++)words[i]=0;
 monitor((char *)2,(char *)32766,words,64,0);burn();monitor(0,0,0,0,0);
 fd=open("mon.out",0);n=read(fd,words,sizeof(words));close(fd);
 check("monitor output",n==sizeof(words) && words[0]==2 && words[1]==32766 && words[2]==0);
 total=0;for(i=3;i<64;i++)total+=words[i];check("monitor histogram",total>0);
 unlink("mon.out");
}
accounting()
{
 struct acct rec;struct stat st;
 int fd,pid,status,i,n,seenfork,seenexec,seenuser;
 unlink("account");fd=creat("account",0600);close(fd);
 check("acct missing",acct("missing")==-1 && errno==ENOENT);
 check("acct directory",acct(".")==-1 && errno==EACCES);
 check("enable accounting",acct("account")==0);
 check("already enabled",acct("account")==-1 && errno==EBUSY);
 pid=fork();if(!pid){burn();_exit(0);}
 check("fork record child",wait(&status)==pid && status==0);
 pid=fork();if(!pid){execl("/bin/services","services","acctexec",0);_exit(90);}
 check("exec record child",wait(&status)==pid && status==0);
 pid=fork();if(!pid){
  if(setgid(21)||setuid(11))_exit(1);
  if(getuid()!=11||geteuid()!=11||getgid()!=21||getegid()!=21)_exit(2);
  if(acct(0)!=-1||errno!=EPERM||lock(1)!=-1||errno!=EPERM)_exit(3);
  _exit(0);
 }
 check("privileged services",wait(&status)==pid && status==0);
 /* Several exit writers contend on the accounting inode. */
 for(i=0;i<4;i++){pid=fork();if(!pid){burn();_exit(0);}check("account fork",pid>0);}
 for(i=0;i<4;i++)check("concurrent exits",wait(&status)>0 && status==0);
 check("disable accounting",acct(0)==0);
 check("disable twice",acct(0)==0);
 fd=open("account",0);n=seenfork=seenexec=seenuser=0;
 while(read(fd,&rec,sizeof(rec))==sizeof(rec)) {
  n++;
  check("record command",!strncmp(rec.ac_comm,"services",8));
  check("record memory/io",rec.ac_mem==0 && rec.ac_io==0);
  if(rec.ac_uid==11) {seenuser++;check("real credentials and ASU",rec.ac_gid==21 && (rec.ac_flag&(AFORK|ASU))==(AFORK|ASU));}
  else if(rec.ac_flag&AFORK) {seenfork++;check("plain fork flags",!(rec.ac_flag&ASU));}
  else seenexec++;
 }
 close(fd);
 check("record count and exec flags",n==7 && seenfork==5 && seenexec==1 && seenuser==1);
 check("record framing",stat("account",&st)==0 && st.st_size==(long)n*sizeof(rec));
 pid=fork();if(!pid)_exit(0);wait(&status);
 check("disabled appends nothing",stat("account",&st)==0 && st.st_size==(long)n*sizeof(rec));
 unlink("account");
}
acctstress()
{
 int fd,gate[2],pid,status,i,n;struct stat st;
 char bytes[512];
 for(i=0;i<2;i++){fd=creat(i?"acctb":"accta",0600);close(fd);}
 pipe(gate);
 check("stress enable",acct("accta")==0);
 for(i=0;i<4;i++) {
  pid=fork();if(!pid){close(gate[1]);read(gate[0],bytes,1);_exit(0);}
  check("stress fork",pid>0);
 }
 close(gate[0]);for(i=0;i<4;i++)write(gate[1],"x",1);close(gate[1]);
 for(i=0;i<16;i++){check("stress disable",acct(0)==0);check("stress reenable",acct(i&1?"accta":"acctb")==0);}
 for(i=0;i<4;i++)check("stress reap",wait(&status)>0 && status==0);
 acct(0);
 n=0;
 for(i=0;i<2;i++) {
  check("stress file",stat(i?"acctb":"accta",&st)==0);
  check("stress complete records",st.st_size%sizeof(struct acct)==0);
  n+=st.st_size/sizeof(struct acct);
 }
 check("stress no duplicate records",n<=4);
 unlink("accta");unlink("acctb");
 fd=creat("account",0600);close(fd);
 fd=creat("fill",0600);while(write(fd,bytes,512)==512);close(fd);
 check("full enable",acct("account")==0);
 pid=fork();if(!pid)_exit(0);
 check("full exit still works",wait(&status)==pid && status==0);
 check("full record rolled back",stat("account",&st)==0 && st.st_size==0);
 unlink("fill");pid=fork();if(!pid)_exit(0);wait(&status);acct(0);
 check("accounting recovers after full",stat("account",&st)==0 && st.st_size==sizeof(struct acct));
 unlink("account");
}
main(argc,argv) char **argv;
{
 long now,stored;struct {int before;long t;int after;} stamp;
 if(argc>1) {burn();return(!strcmp(argv[1],"profexec") && samples[0]!=0);}
 chdir("/tmp");
 check("public types",sizeof(label_t)==24 && sizeof(jmp_buf)==24 && USIZE==64);
 check("public registers",UREG_NREG==19 && PC==16 && UREG_PC==18);
 stamp.before=123;stamp.after=456;stamp.t= -1;
 now=time(&stamp.t);stored=time(0);
 check("time stores and returns long",now==stamp.t && stored>=now && stamp.before==123 && stamp.after==456);
 check("root IDs",getuid()==0 && geteuid()==0 && getgid()==0 && getegid()==0);
 check("root lock",lock(1)==0);
 burn();check("root unlock",lock(0)==0);
 profile();histtest();accounting();acctstress();
 printf("services: %s\n",failed?"FAILED":"passed");return(failed!=0);
}
