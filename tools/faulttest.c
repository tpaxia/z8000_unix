#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
extern int errno;
long lseek(), time();
int failed;
check(name, ok) char *name;
{
	if (!ok) { printf("fault: FAIL %s errno=%d\n",name,errno); failed++; }
}
handler() { _exit(99); }
main(argc, argv) int argc; char **argv;
{
	int fd, n, i, pid, status;
	char c, *p, *q, *args[2];
	char data[16];
	long t;
	struct stat st;
	if(argc != 2) return(1);
	p=(char *)0x9000;
	for(i=0;i<16;i++) { p[i]='a'+i; data[i]='a'+i; }
	p[-2]='x'; p[-1]='y';
	fd=creat("/tmp/fault",0600);
	check("create",fd>=0 && write(fd,data,16)==16); close(fd);
	fd=open("/tmp/fault",2);
	check("open",fd>=0);
	if(strcmp(argv[1],"bounds")==0) {
		for(i=0;i<2;i++) {
			errno=0;
			check("write wrap",write(fd,(char *)(0xfffe+i),4)==-1 && errno==EFAULT);
			errno=0;
			check("read wrap",read(fd,(char *)(0xfffe+i),4)==-1 && errno==EFAULT);
		}
		check("offset unchanged",lseek(fd,0L,1)==0L);
		errno=0;
		check("bulk wrap",fstat(fd,(char *)0xffff)==-1 && errno==EFAULT);
		check("zero length",read(fd,(char *)0xffff,0)==0 && write(fd,(char *)0xffff,0)==0);
		*(char *)0xffff='z';
		check("last byte",write(fd,(char *)0xffff,1)==1);
		lseek(fd,0L,0);
		check("last byte read",read(fd,(char *)0xffff,1)==1 && *(char *)0xffff=='z');
		errno=0;
		check("odd vector",execve("/bin/faultn",(char *)0xffff,0)==-1 && errno==EFAULT);
	} else {
		printf("fault: ready\n"); fflush(stdout);
		read(0,&c,1);
		for(i=0;i<3;i++) {
			lseek(fd,0L,0); errno=0;
			if(strncmp(argv[1],"write",5)==0 || strncmp(argv[1],"read",4)==0) {
				int writing, partial, bytes;
				writing=argv[1][0]=='w';
				bytes=strncmp(argv[1]+(writing?5:4),"byte",4)==0;
				partial=strcmp(argv[1]+strlen(argv[1])-4,"part")==0;
				n=bytes ? 3 : 4; q=partial ? p-2 : p;
				check("user copy fault",(writing ? write(fd,q,n) : read(fd,q,n))==-1 && errno==EFAULT);
				check("partial offset",lseek(fd,0L,1)==(partial && bytes ? 2L : 0L));
			} else if(strcmp(argv[1],"path")==0)
				check("pathname",open(p,0)==-1 && errno==EFAULT);
			else if(strcmp(argv[1],"argv")==0)
				check("argument vector",execve("/bin/faultn",p,0)==-1 && errno==EFAULT);
			else if(strcmp(argv[1],"epu")==0)
				check("EPU restore",epufault(p)==0);
			else {
				pid=fork();
				if(pid==0) {
					if(strcmp(argv[1],"signal")==0) {
						signal(SIGTERM,handler); sigfault(getpid());
					} else if(strcmp(argv[1],"exec")==0) {
						args[0]="faultbig";args[1]=0;
						execve("/bin/faultbig",args,0);
					} else _exit(*p);
					_exit(98);
				}
				check("faulted child",pid>0 && wait(&status)==pid &&
				    status==(strcmp(argv[1],"exec")==0 ? SIGKILL : SIGSEGV));
			}
			/* A successful copy after each trap checks stack/mode restoration. */
			check("subsequent syscall",fstat(fd,&st)==0 && getpid()>0);
		}
	}
	close(fd);
	/* Clock interrupts must remain enabled after recovery. */
	t=time(0); sleep(2); check("clock after fault",time(0)>=t+1);
	printf("fault: %s\n",failed ? "FAILED" : "passed");
	return(failed!=0);
}
