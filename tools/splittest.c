#include <stdio.h>
#include <signal.h>
#include <errno.h>

extern int errno;
char arena[32000];
int initial = 1234;
int highcall();
int (*callhigh)() = highcall;
int seen;

handler(sig) { seen = sig; }

/* Dense cases force a jump table, which must be read from data space. */
choose(n)
{
	switch(n) {
	case 0: return(13);
	case 1: return(21);
	case 2: return(34);
	case 3: return(55);
	case 4: return(89);
	default: return(144);
	}
}

main()
{
	int i, fd, pid, status;
	char *args[3];
	unsigned header[8];

	if (initial != 1234 || callhigh() != 42 || choose(3) != 55 || choose(9) != 144)
		return(1);
	for (i=0; i<32000; i++) {
		if (arena[i]) return(2);
		arena[i] = i % 113;
	}
	fd = creat("/tmp/iddata", 0600);
	if (fd < 0 || write(fd, arena, 32000) != 32000) return(3);
	close(fd);
	fd = open("/tmp/iddata", 0);
	if (fd < 0 || read(fd, arena, 32000) != 32000) return(4);
	close(fd);
	for (i=0; i<32000; i++) if (arena[i] != i%113) return(5);
	if (signal(SIGINT, handler) == (int (*)())-1) return(6);
	kill(getpid(), SIGINT);
	if (seen != SIGINT) return(7);

	/* Failed exec must preserve both the old code and old data. */
	for (i=0; i<8; i++) header[i]=0;
	header[0]=0411; header[1]=2; header[2]=65534; header[3]=4;
	fd=creat("/tmp/badid",0700);
	write(fd,header,16); close(fd);
	args[0]="badid"; args[1]=0;
	if (execve("/tmp/badid",args,0) != -1 || errno != ENOEXEC) return(8);

	pid=fork();
	if (pid<0) return(9);
	if (pid==0) {
		if (arena[31999] != 31999%113 || callhigh()!=42) exit(10);
		arena[31999]=0;
		args[0]="idnext"; args[1]="split"; args[2]=0;
		execve("/bin/idnext",args,0);
		exit(11);
	}
	if (wait(&status)!=pid || status || arena[31999]!=31999%113) return(12);
	pid=fork();
	if (pid<0) return(13);
	if (pid==0) {
		args[0]="idnormal"; args[1]="normal"; args[2]=0;
		execve("/bin/idnormal",args,0);
		exit(14);
	}
	if (wait(&status)!=pid || status || callhigh()!=42) return(15);
	printf("split: all checks passed\n");
	return(0);
}
