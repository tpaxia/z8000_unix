/* Exercise real exec credentials and CPU startup through V7 syscalls. */
#include <sys/param.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <stdio.h>
#include <sys/stat.h>
extern int errno;
struct user layout;
int failed;
char *av[9];
char *env[] = {"EXEC_ENV=ok", 0};
check(name, ok) char *name;
{ if (!ok) {printf("exec: FAIL %s errno=%d\n", name, errno); failed++;} }
handler() {}
word(pid, field) int *field;
{ return(ptrace(3, pid, (char *)field-(char *)&layout, 0)); }

/* File ownership observes effective IDs independently of ptrace's u-area. */
identity(ru, eu, rg, eg)
{
 int fd; struct stat st;
 fd=creat("owned",0666);
 if(fd<0) return(1);
 if(fstat(fd,&st)<0 || st.st_uid!=eu || st.st_gid!=eg ||
    getuid()!=ru || geteuid()!=eu || getgid()!=rg || getegid()!=eg) {close(fd);return(2);}
 close(fd);return(0);
}
target(argc, argv) char **argv;
{
 int ru,eu,rg,eg,pid,status,parent;
 ru=atoi(argv[2]);eu=atoi(argv[3]);rg=atoi(argv[4]);eg=atoi(argv[5]);
 if(argc!=8 || strcmp(argv[7],"last") || strcmp(env[0],"EXEC_ENV=ok")) return(3);
 if(identity(ru,eu,rg,eg)) return(4);
 if(signal(SIGINT,1)!=1 || signal(SIGTRM,1)!=0) return(5);
 /* Real UID is unchanged; p_uid must track effective UID for kill access. */
 if(ru!=eu) {
  parent=getpid();pid=fork();
  if(!pid) {_exit(setuid(ru)!=0 || kill(parent,0)!=-1 || errno!=ESRCH);}
  if(pid<0 || wait(&status)!=pid || status) return(6);
 }
 if(!strcmp(argv[6],"core")) {kill(getpid(),SIGQUIT);return(7);}
 if(!strcmp(argv[6],"chain")) {
  unlink("owned");argv[6]="report";
  signal(SIGTRM,handler);
  execve("/bin/plain",argv,env);return(8);
 }
 return(0);
}
run(path, root, traced, ru, eu, rg, eg, mode)
char *path, *ru, *eu, *rg, *eg, *mode;
{
 int pid,status,fd,i,value;char bytes[5];struct stat st;
 unlink("owned");unlink("core");
 fd=creat("core",0666);write(fd,"keep",4);close(fd);
 av[0]="exectest";av[1]="target";av[2]=ru;av[3]=eu;
 av[4]=rg;av[5]=eg;av[6]=mode;av[7]="last";av[8]=0;
 pid=fork();
 if(!pid) {
  if(!root && (setgid(20) || setuid(10))) _exit(90);
  signal(SIGINT,1);signal(SIGTRM,handler);
  if(traced && ptrace(0,0,0,0)<0) _exit(91);
  execve(path,av,env);_exit(92);
 }
 check("fork",pid>0);if(pid<0)return;
 if(traced) {
  check("exec trace stop",wait(&status)==pid && status==((SIGTRC<<8)|0177));
  if(status!=((SIGTRC<<8)|0177)) return;
  check("traced real uid",word(pid,&layout.u_ruid)==atoi(ru));
  check("traced effective uid",word(pid,&layout.u_uid)==atoi(eu));
  check("traced real gid",word(pid,&layout.u_rgid)==atoi(rg));
  check("traced effective gid",word(pid,&layout.u_gid)==atoi(eg));
  for(i=0;i<15;i++) check("exec clears all general registers",word(pid,&layout.u_regs[i])==0);
  check("entry PC",word(pid,&layout.u_regs[18])==0);
  value=word(pid,&layout.u_regs[15]);
  check("initial stack argc",!(value&1) && ptrace(2,pid,value,0)==8);
  for(i=0;i<96;i+=2)
   check("exec clears EPU",word(pid,(int *)(layout.u_fpe+i))==(i==92?1:0));
  check("trace continue",ptrace(7,pid,1,0)==0);
 }
 check("wait",wait(&status)==pid);
 if(!strcmp(mode,"core")) {
  i=atoi(ru)!=atoi(eu) || atoi(rg)!=atoi(eg);
  check("credential core status",status==(i?SIGQUIT:SIGQUIT|0200));
  if(i) {
   fd=open("core",0);value=read(fd,bytes,5);close(fd);
   check("set-ID core unchanged",value==4 && !strncmp(bytes,"keep",4));
  } else check("ordinary core written",stat("core",&st)==0 && st.st_size>4096L);
 } else {
  if(status) printf("exec: target %s status %d\n",path,status);
  check("target credentials and startup",status==0);
 }
}
failedexec(path, error) char *path;
{
 int pid,status;
 unlink("owned");pid=fork();
 if(!pid) {
  setgid(20);setuid(10);
  execl(path,"bad",(char *)0);
  if(errno!=error) _exit(1);
  _exit(identity(10,10,20,20));
 }
 check("failed exec preserves credentials",pid>0 && wait(&status)==pid && status==0);
}
main(argc,argv,envp) char **argv, **envp;
{
 if(argc>1) {env[0]=envp[0];return(target(argc,argv));}
 chdir("/tmp");umask(0);
 run("/bin/plain",0,0,"10","10","20","20","report");
 run("/bin/suid",0,0,"10","11","20","20","report");
 run("/bin/sgid",0,0,"10","10","20","21","report");
 run("/bin/both",0,0,"10","11","20","21","report");
 run("/bin/root",0,0,"10","0","20","20","report");
 run("/bin/both",1,0,"0","0","0","21","report");
 run("/bin/both",0,0,"10","11","20","21","chain");
 run("/bin/both",0,1,"10","10","20","20","report");
 run("/bin/root",0,1,"10","10","20","20","report");
 run("/bin/suid",0,0,"10","11","20","20","core");
 run("/bin/sgid",0,0,"10","10","20","21","core");
 run("/bin/root",0,0,"10","0","20","20","core");
 run("/bin/plain",0,0,"10","10","20","20","core");
 failedexec("/bin/bad",ENOEXEC);
 failedexec("/bin/denied",EACCES);
 unlink("owned");unlink("core");
 printf("exec: %s\n",failed?"FAILED":"passed");return(failed!=0);
}
