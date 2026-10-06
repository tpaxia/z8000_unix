#include <stdio.h>
char buf[32], hex[65];
char digits[]="0123456789abcdef";
main()
{
 int pid,status,fd,n,i,c,phase;
 char *args[2];
 for(phase=0;phase<2;phase++) {
 pid=fork();
 if(pid<0) return 1;
 if(pid==0) {
  close(0); if(open(phase ? "/tmp/middle" : "/tmp/input",0)!=0) exit(2);
  close(1); if(creat(phase ? "/tmp/output" : "/tmp/middle",0600)!=1) exit(3);
  args[0]="back"; args[1]=0;
  execve(phase ? "/bin/back" : "/bin/front",args,0); exit(4);
 }
 if(wait(&status)!=pid || status) {printf("back failed %d\n",status);return 5;}
 }
 fd=open("/tmp/output",0); if(fd<0)return 6;
 printf("OUTPUT BEGIN\n"); fflush(stdout);
 while((n=read(fd,buf,32))>0) {
  for(i=0;i<n;i++) {c=buf[i]&255;hex[2*i]=digits[c>>4];hex[2*i+1]=digits[c&15];}
  hex[2*n]='\n';write(1,hex,2*n+1);
 }
 close(fd);
 printf("OUTPUT END\n");return 0;
}
