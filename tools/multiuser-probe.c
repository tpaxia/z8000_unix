#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/proc.h>
#include <a.out.h>
#include <sys/param.h>
#include <sgtty.h>
char *getlogin(), *ttyname(), *crypt();
struct nlist names[] = {{"_proc"}, {0}};
fail(code)
int code;
{ printf("MULTIUSER PROBE FAIL %d\n",code);return code; }
main(argc, argv)
int argc;
char **argv;
{
 struct stat st;
 struct proc p;
 struct sgttyb tty;
 int fd, n, active, parent;
 int uid;
 if(argc>1 && strcmp(argv[1],"su")==0) {
  if(getuid() || geteuid() || getgid() || getegid())return fail(12);
  puts("MULTIUSER SETUID PASS");return 0;
 }
 uid = argc > 1 ? 100 : 0;
 if(getuid()!=uid || geteuid()!=uid || getgid()!=(uid?10:0) || getegid()!=(uid?10:0))return fail(1);
 if(!getlogin() || strcmp(getlogin(),uid?"test":"root"))return fail(2);
 if(!ttyname(0) || strcmp(ttyname(0),"/dev/console"))return fail(3);
 if(stat("/dev/console",&st)<0 || st.st_uid!=uid || (st.st_mode&0777)!=0622)return fail(4);
 if(gtty(0,&tty)<0 || !(tty.sg_flags&ECHO) || tty.sg_erase!='\b')return fail(5);
 fd=open("/dev/tty",0);
 if(fd<0 || close(fd)<0 || gtty(0,&tty)<0 || !(tty.sg_flags&ECHO))return fail(13);
 fd=open("/dev/kmem",0);
 if(uid) {
  if(fd>=0)return fail(6);
  fd=creat("proof",0600);
  if(fd<0 || write(fd,"USER-WRITE\n",11)!=11 || close(fd)<0)return fail(7);
  puts("MULTIUSER USER PASS");
 } else {
  if(fd<0 || nlist("/unix",names)<0 || !names[0].n_value)return fail(8);
  lseek(fd,(long)names[0].n_value,0);
  active=0; parent=0;
  for(n=0;n<NPROC;n++) {if(read(fd,&p,sizeof(p))!=sizeof(p))return fail(9);if(p.p_stat)active++; if(p.p_pid==getpid())parent=(p.p_pgrp==p.p_ppid);}
  close(fd);
  printf("active processes: %d\n",active);
  if(active>12 || !parent)return fail(10);
  if(strcmp(crypt("password","ab"),"abJnggxhB/yWI"))return fail(11);
  puts("MULTIUSER ROOT PASS");
 }
 return 0;
}
