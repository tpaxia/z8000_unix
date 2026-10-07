/* Raw ATA syscall tests. Scratch sectors are appended outside the filesystem. */
#include <stdio.h>
#include <errno.h>
extern int errno;
extern long lseek();
int failed, storage[2560];
check(s,ok) char *s;
{ if(!ok) { printf("raw: FAIL %s errno=%d\n",s,errno);failed++; } }
seekto(fd,sector) { check("seek",lseek(fd,(long)sector*512,0)==(long)sector*512); }
worker(n)
{
 int fd,i,j; int words[512]; char *p;
 p=(char *)words;fd=open("/dev/rhd",2);
 if(fd<0) return(1);
 for(j=0;j<3;j++) {
  for(i=0;i<512;i++) p[i]=n+30+j;
  if(lseek(fd,(long)(1604+n)*512,0)<0 || write(fd,p,512)!=512) return(2);
  for(i=0;i<512;i++) p[i]=0;
  if(lseek(fd,(long)(1604+n)*512,0)<0 || read(fd,p,512)!=512) return(3);
  for(i=0;i<512;i++) if(p[i]!=n+30+j) return(4);
 }
 close(fd);return(0);
}
main()
{
 int fd,i,n,pid,status,gate[2];char *p,byte;int stack[512];
 p=(char *)storage;
 /* Start 256 bytes before a page boundary, forcing split sector copies. */
 p+=(2048-((unsigned)p&2047)+1792)&2047;
 fd=open("/dev/rhd",2);check("raw open",fd>=0);if(fd<0) return(1);
 for(i=0;i<1536;i++) p[i]=i%113;
 seekto(fd,1600);check("three sector write",write(fd,p,1536)==1536);
 for(i=0;i<1536;i++) p[i]=0;
 seekto(fd,1600);check("three sector read",read(fd,p,1536)==1536);
 for(i=0;i<1536;i++) if(p[i]!=i%113) {check("data bytes",0);break;}
 seekto(fd,1600);check("stack read",read(fd,(char *)stack,1024)==1024);
 for(i=0;i<1024;i++) if(((char *)stack)[i]!=i%113) {check("stack bytes",0);break;}
 seekto(fd,1600);errno=0;check("odd buffer",read(fd,p+1,512)==-1 && errno==EFAULT);
 errno=0;check("gap buffer",read(fd,(char *)0x8000,512)==-1 && errno==EFAULT);
 errno=0;check("wrapping buffer",read(fd,(char *)0xff00,512)==-1 && errno==EFAULT);
 errno=0;check("partial sector",read(fd,p,511)==-1 && errno==EINVAL);
 lseek(fd,1L,0);errno=0;check("unaligned offset",write(fd,p,512)==-1 && errno==EINVAL);
 seekto(fd,1600);check("zero length",read(fd,p,0)==0);
 seekto(fd,1615);for(i=0;i<1024;i++) p[i]=77;
 check("last sector write",write(fd,p,512)==512);
 seekto(fd,1615);for(i=0;i<1024;i++) p[i]=99;errno=0;
 check("partial read error",read(fd,p,1024)==-1 && errno==EIO);
 check("partial read offset",lseek(fd,0L,1)==1616L*512);
 for(i=0;i<1024;i++) if(p[i]!=(i<512?77:99)) {check("partial read bytes",0);break;}
 seekto(fd,1615);errno=0;
 check("partial write error",write(fd,p,1024)==-1 && errno==EIO);
 check("partial write offset",lseek(fd,0L,1)==1616L*512);
 seekto(fd,1600);check("recovery after error",read(fd,p,512)==512);
 close(fd);errno=0;check("swap protected",open("/dev/rswap",2)==-1 && errno==EBUSY);
 errno=0;check("bad minor",open("/dev/rbad",0)==-1 && errno==ENXIO);
 check("pipe",pipe(gate)==0);n=0;
 for(i=0;i<8;i++) {
  pid=fork();
  if(pid==0) {close(gate[1]);if(read(gate[0],&byte,1)!=1) exit(5);exit(worker(i));}
  if(pid<0) {check("pressure fork",0);break;} n++;
 }
 close(gate[0]);for(i=0;i<n;i++) write(gate[1],"x",1);close(gate[1]);
 for(i=0;i<n;i++) {check("wait",wait(&status)>0);check("parallel raw IO",status==0);}
 printf("raw: %s\n",failed?"FAILED":"passed");return(failed!=0);
}
