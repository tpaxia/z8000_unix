/* Recover the emulated board's reserved crash area using ordinary V7 I/O. */
#include <stdio.h>
long lseek();
char block[512];
long number(p) unsigned char *p;
{return ((long)p[0]<<24)|((long)p[1]<<16)|((long)p[2]<<8)|p[3];}
fail(s) char *s; {fprintf(stderr,"savecore: %s\n",s);exit(1);}
getblock(fd,off) long off;
{if(lseek(fd,off,0)<0 || read(fd,block,512)!=512)fail("read dump");}
copy(fd,off,size,name) long off,size; char *name;
{
 int out;
 out=creat(name,0600);if(out<0)fail(name);
 if(chmod(name,0600)<0)fail("protect recovered image");
 while(size) {
  getblock(fd,off);
  if(write(out,block,512)!=512)fail("write recovered image");
  off+=512;size-=512;
 }
 if(close(out)<0)fail("close recovered image");
}
main(argc,argv) char **argv;
{
 int fd;
 long base,ram,swap;
 char *device,*directory;
 device=argc>1?argv[1]:"/dev/rhd";
 directory=argc>2?argv[2]:"/usr/sys";
 fd=open(device,0);if(fd<0)fail(device);
 getblock(fd,512L);base=number(block+2)*512L;
 if(base<1536L || base>33553920L)fail("filesystem boundary");
 getblock(fd,base);
 if(number(block)!=0x5a384b44L || number(block+4)!=1L)fail("no complete crash dump");
 ram=number(block+8);swap=number(block+12);
 if(ram<196608L || ram>8388608L || (ram&511) ||
    swap<0 || swap>16384000L || (swap&511) ||
    base+512L+ram+swap>33554432L)fail("dump lengths");
 if(chdir(directory)<0)fail(directory);
 copy(fd,base+512L,ram,"core");
 copy(fd,base+512L+ram,swap,"swap");
 close(fd);sync();printf("savecore: %D RAM bytes, %D swap bytes recovered\n",ram,swap);
 return 0;
}
