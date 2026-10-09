/* Live memory-device checks; writes restore the reserved bytes immediately. */
#include <stdio.h>
#include <a.out.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <errno.h>
extern int errno;
struct nlist nl[]={{"_msgbuf"},{"_physmem"},{"_proc"},{""}};
struct proc p[NPROC];
char old[8], data[8], actual[8];
long lseek();
fail(n) {printf("inspection probe failed %d errno %d\n",n,errno);exit(n);}
main(argc,argv)
char **argv;
{
 int k,m,i,n,pid,status;
 char frame[16], *args[4];
 unsigned frames;
 long offset;
 if(nlist("/unix",nl)<0 || !nl[0].n_type || !nl[1].n_type || !nl[2].n_type)fail(1);
 k=open("/dev/kmem",2);m=open("/dev/mem",2);
 if(k<0 || m<0)fail(2);
 lseek(k,(long)nl[2].n_value,0);
 if(read(k,(char *)p,sizeof p)!=sizeof p)fail(3);
 if(argc>1) {
  sprintf(frame,"%o",(unsigned)p[0].p_addr);
  args[0]="pstat";args[1]="-u";args[2]=frame;args[3]=0;
  execve("/bin/pstat",args,0);fail(4);
 }
 /* Kernel data symbols are virtual offsets, physical kernel data is bank 1. */
 offset=nl[0].n_value;
 lseek(k,offset,0);lseek(m,65536L+offset,0);
 if(read(k,old,8)!=8 || read(m,actual,8)!=8)fail(5);
 for(i=0;i<8;i++)if(old[i]!=actual[i])fail(6);
 lseek(k,offset,0);if(write(k,old,8)!=8)fail(7);
 /* Reserved low RAM below the PSA: exercise a 2 KiB window boundary. */
 offset=2044L;
 lseek(m,offset,0);if(read(m,old,8)!=8)fail(8);
 for(i=0;i<8;i++)data[i]=i+128;
 lseek(m,offset,0);if(write(m,data,8)!=8)fail(9);
 lseek(m,offset,0);n=read(m,actual,8);
 lseek(m,offset,0);if(write(m,old,8)!=8 || n!=8)fail(10);
 for(i=0;i<8;i++)if(data[i]!=actual[i])fail(11);
 lseek(k,(long)nl[1].n_value,0);if(read(k,(char *)&frames,2)!=2)fail(12);
 lseek(m,(long)frames*2048L,0);if(read(m,data,1)!=-1 || errno!=ENXIO)fail(13);
 lseek(k,65536L,0);if(read(k,data,1)!=-1 || errno!=ENXIO)fail(14);
 lseek(k,0L,0);if(read(k,data,0)!=0 || write(k,data,0)!=0)fail(15);
 lseek(k,0L,0);if(read(k,(char *)0x8000,1)!=-1 || errno!=EFAULT)fail(16);
 if(mknod("/dev/badmem",020600, (4<<8)|3)<0)fail(17);
 if(open("/dev/badmem",0)!=-1 || errno!=ENXIO)fail(18);
 pid=fork();if(pid<0)fail(19);
 if(!pid) {
  if(setuid(1)<0)exit(1);
  if(open("/dev/mem",0)!=-1 || open("/dev/kmem",0)!=-1)exit(2);
  exit(0);
 }
 if(wait(&status)!=pid || status)fail(20);
 close(k);close(m);
 puts("inspection memory: passed");return 0;
}
