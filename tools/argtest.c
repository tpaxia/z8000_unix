/* Swap-backed exec: boundaries, failed reservation cleanup and isolation. */
#include <sys/param.h>
#include <stdio.h>
#include <errno.h>
extern int errno;
char bytes[NCARGS+1];
char *args[4], *envs[2];
char tag[]="T0";
int failed;
check(name, ok) char *name;
{ if(!ok) {printf("args: FAIL %s errno=%d\n",name,errno);failed++;} }
fill(n, seed)
{
 int i;
 for(i=0;i<n;i++) bytes[i]=1+(i+seed)%255;
 bytes[n]=0;
}
main(argc,argv,envp) char **argv, **envp;
{
 int i,j,pid,status,n,seed,count; char *path;
 if(argc==0) return(envp[0]!=0);
 if(argc==2 && argv[0][0]=='T') {
  seed=argv[0][1]-'0';n=NCARGS-5;
  if(envp[0]) return(2);
  for(i=0;i<n;i++) if((argv[1][i]&0377)!=1+(i+seed)%255) return(3);
  return(argv[1][n]!=0);
 }
 if(argc==1 && argv[0][0]=='E')
  return(!envp[0] || strcmp(envp[0],"ENV=present") || envp[1]!=0);
 args[0]=tag;args[1]=bytes;args[2]=0;envs[0]=0;
 /* 3-byte argv[0] plus a 5116-byte string = V7's 5119-byte maximum. */
 for(i=0;i<40;i++) {
  fill(NCARGS-4,0);
  execve("/bin/arga",args,envs);
  check("E2BIG and reservation reuse",errno==E2BIG);
  args[1]=(char *)65535;
  execve("/bin/arga",args,envs);
  check("bad string",errno==EFAULT);
  args[1]=bytes;
  execve("/bin/arga",(char **)65535,envs);
  check("bad vector",errno==EFAULT);
 }
 count=argc>1?1:3;
 for(i=0;i<count;i++) {
  pid=fork();
  if(!pid) {
   args[0]=tag;args[0][1]='0'+i;fill(NCARGS-5,i);
   path=i==0?"/bin/arga":i==1?"/bin/argb":"/bin/argc";
   execve(path,args,envs);_exit(4);
  }
  check("concurrent fork",pid>0);
 }
 for(i=0;i<count;i++) check("concurrent maximum args",wait(&status)>0 && status==0);
 for(j=0;j<2;j++) {
  pid=fork();
  if(!pid) {
   args[0]="E";args[1]=0;envs[0]="ENV=present";envs[1]=0;
   execve("/bin/arga",j?args:(char **)0,envs);_exit(5);
  }
  check("V7 null argv and environment",pid>0 && wait(&status)==pid && status==0);
 }
 printf("args: %s\n",failed?"FAILED":"passed");return(failed!=0);
}
