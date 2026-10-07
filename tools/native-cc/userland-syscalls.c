/* Exercise the new libc entry points, including nonzero high time bits. */
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
extern int errno;
long time();
char *sbrk();
command(path, a, b)
char *path, *a, *b;
{
 int pid, status;
 pid=fork();
 if(!pid) {execl(path,path,a,b,(char *)0);_exit(127);}
 if(pid<0 || wait(&status)!=pid) return(-1);
 return(status);
}
main()
{
 long saved, value, now;
 int pid, status, fd;
 char byte;
 struct stat st;
 char *base, *top;
 fd=open("/dev/null",2);
 if(fd<0 || read(fd,&byte,1)!=0 || write(fd,"abc",3)!=3 ||
    read(fd,&byte,1)!=0 || close(fd)<0) return(20);
 if(mknod("/tmp/nomem",0020600,4<<8)<0) return(21);
 if(open("/tmp/nomem",0)!=-1 || errno!=ENXIO) return(22);
 if(unlink("/tmp/nomem")<0) return(23);
 base=sbrk(0);
 if(brk(base+8192)<0 || sbrk(0)!=base+8192) return(15);
 base[4096]=77;
 if(sbrk(512)!=base+8192 || base[4096]!=77) return(16);
 top=sbrk(0);
 if(brk((char *)65535)!=-1 || sbrk(0)!=top) return(17);
 if(brk(base)<0 || sbrk(0)!=base) return(18);
 saved=time((long *)0);value=0x00123456L;
 if(stime(&value)<0) return(1);
 now=time((long *)0);
 if(now<value || now>value+3) return(2);
 if(stime(&saved)<0) return(3);
 unlink("/tmp/node");
 if(mknod("/tmp/node",0020600,0)<0 || stat("/tmp/node",&st)<0 ||
    (st.st_mode&0170000)!=0020000 || st.st_rdev!=0) return(4);
 if(unlink("/tmp/node")<0) return(5);
 pid=fork();
 if(!pid) {
  if(setgid(10)<0 || setuid(10)<0) _exit(6);
  if(stime(&value)!=-1 || errno!=EPERM) _exit(7);
  if(mknod("/tmp/denied",0020600,0)!=-1 || errno!=EPERM) _exit(8);
  if(command("/bin/mkdir","/tmp/userdir",(char *)0)) _exit(10);
  if(stat("/tmp/userdir",&st)<0 || st.st_uid!=10 || st.st_gid!=10) _exit(11);
  if(command("/bin/mv","/tmp/userdir","/tmp/moved")) _exit(12);
  if(command("/bin/rmdir","/tmp/moved",(char *)0)) _exit(13);
  if(stat("/tmp/moved",&st)!=-1 || errno!=ENOENT) _exit(19);
  if(!command("/bin/mkdir","/bin/denied",(char *)0)) _exit(14);
  _exit(0);
 }
 if(pid<0 || wait(&status)!=pid || status) return(9);
 puts("USERLAND SYSCALLS OK");return(0);
}
