/* Hold private memory across swapping while the login shell builds programs. */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <a.out.h>
char *sbrk();
long lseek();
struct nlist names[] = {{"_proc"}, {0}};
fail(n)
int n;
{ printf("LOAD FAIL %d\n",n);fflush(stdout);return n; }
main(argc, argv)
char **argv;
{
 int p[2], i, pid, status, fd, swapped;
 unsigned j;
 char *data, c;
 struct stat st;
 struct proc table[NPROC];
 if(argc > 1) {
  if(nlist("/unix", names)<0 || !names[0].n_value)return 10;
  fd=open("/dev/kmem",0);if(fd<0)return 11;
  lseek(fd,(long)names[0].n_value,0);
  if(read(fd,(char *)table,sizeof table)!=sizeof table)return 12;
  close(fd);swapped=0;
  for(i=0;i<NPROC;i++)if(table[i].p_stat && table[i].p_stat!=SZOMB &&
      !(table[i].p_flag&SLOAD))swapped++;
  printf("LOAD SWAPPED %d\n",swapped);
  return swapped ? 0 : 13;
 }
 for(i=0;i<4;i++) {
  if(pipe(p)<0)return fail(1);
  pid=fork();if(pid<0)return fail(2);
  if(!pid) {
   close(p[0]);data=sbrk(24000);
   if(data==(char *)-1)exit(fail(3));
   if(sbrk(24000)!=data+24000)exit(fail(3));
   for(j=0;j<48000;j++)data[j]=(j+i)&127;
   c=i;if(write(p[1],&c,1)!=1)exit(fail(4));close(p[1]);
   while(stat("/usr/adm/release",&st)<0)sleep(30);
   for(j=0;j<48000;j++)if(data[j]!=((j+i)&127))exit(fail(5));
   exit(0);
  }
  /* Fill each holder before starting the next, so setup itself does not
   * create four CPU-bound images competing to be swapped in at once. */
  close(p[1]);
  if(read(p[0],&c,1)!=1 || c!=i)return fail(6);
  close(p[0]);
 }
 puts("LOAD READY");fflush(stdout);
 /* Notify the harness directly: polling with external test/sleep commands
  * would repeatedly swap in competing images during low-RAM setup. */
 fd=open("/dev/tty",1);
 if(fd<0 || write(fd,"LOAD READY\n",11)!=11)return fail(8);
 close(fd);
 for(i=0;i<4;i++)if(wait(&status)<0 || status){printf("child status %d\n",status);return fail(7);}
 puts("LOAD MEMORY PASS");return 0;
}
