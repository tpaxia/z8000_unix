/* Force nonresident processes so ps must read paged swap images too. */
#include <stdio.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <a.out.h>
char *sbrk();
long lseek();
main(argc,argv)
char **argv;
{
 int p[2],i,pid,status,k,swapped,children[9];
 char *data,c,*args[3];
 static struct nlist nl[]={{"_proc"},{""}};
 struct proc table[NPROC];
 if(pipe(p)<0)return 1;
 for(i=0;i<9;i++) {
  pid=fork();if(pid<0)return 2;
  if(!pid) {
   close(p[0]);data=sbrk(32760);
   if(data==(char *)-1)exit(3);
   data[0]=data[32759]='a'+i;
   c='a'+i;write(p[1],&c,1);close(p[1]);
   for(;;)pause();
  }
  children[i]=pid;
 }
 close(p[1]);for(i=0;i<9;i++)if(read(p[0],&c,1)!=1)return 4;
 close(p[0]);
 if(nlist("/unix",nl)<0 || !nl[0].n_type)return 5;
 k=open("/dev/kmem",0);if(k<0)return 6;
 lseek(k,(long)nl[0].n_value,0);
 if(read(k,(char *)table,sizeof table)!=sizeof table)return 7;
 close(k);swapped=0;
 for(i=0;i<NPROC;i++)if(table[i].p_stat && table[i].p_stat!=SZOMB &&
     !(table[i].p_flag&SLOAD))swapped++;
 printf("inspection pressure: %d swapped\n",swapped);fflush(stdout);
 if(!swapped)return 8;
 pid=fork();if(pid<0)return 9;
 if(!pid) {args[0]="ps";args[1]="axl";args[2]=0;execve("/bin/ps",args,0);exit(10);}
 if(wait(&status)!=pid || status)return 11;
 if(argc>1) {
  /* Leave a real traced stop and an unreaped zombie in the frozen table. */
  pid=fork();if(pid<0)return 12;
  if(!pid) {
   if(ptrace(0,0,0,0)<0)exit(13);
   kill(getpid(),5);exit(14);
  }
  if(wait(&status)!=pid || (status&255)!=0177)return 15;
  pid=fork();if(pid<0)return 16;
  if(!pid)exit(17);
  k=open("/dev/kmem",0);if(k<0)return 18;
  for(i=0;i<60;i++) {
   lseek(k,(long)nl[0].n_value,0);
   if(read(k,(char *)table,sizeof table)!=sizeof table)return 19;
   for(status=0;status<NPROC;status++)
    if(table[status].p_pid==pid && table[status].p_stat==SZOMB)break;
   if(status<NPROC)break;
   sleep(1);
  }
  close(k);if(i==60)return 20;
  puts("inspection dump: ready");fflush(stdout);
  for(;;)pause();
 }
 for(i=0;i<9;i++)kill(children[i],SIGKIL);
 for(i=0;i<9;i++)wait(&status);
 puts("inspection swap: passed");
 return 0;
}
