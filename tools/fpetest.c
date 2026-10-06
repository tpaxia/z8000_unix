#include <stdio.h>
#include <signal.h>
double value=3.25, other= -17.5;
int seen, failed;
double atof();
check(name,ok)
char *name;
int ok;
{ if(!ok) { printf("fpe: FAIL %s\n",name);failed++; } }
handler(sig)
{
	double x;
	x=other*value;
	if(x!= -56.875) failed++;
	seen++;
}
main(argc,argv)
int argc;
char **argv;
{
	double x;
	unsigned long ul;
	long sl;
	int pid,status,i;
	char *args[3];
	if(argc>1) { fpget(&x);return x!=0.0; }
	ul=4294967295L;sl= -2147483647L;
	check("unsigned long input",(double)ul==4294967295.0);
	check("signed long input",(double)sl== -2147483647.0);
	x=4294967295.0;check("unsigned long output",(unsigned long)x==ul);
	x= -2147483647.75;check("signed truncation",(long)x==sl);
	check("atof",atof("3.25")==value);
	fpmem(&x,&value);check("EPU memory operands",x==value);
	fpedge(&x,&value);check("EPU direct address and segment end",x==value);
	fpseed(&value);
	pid=fork();check("fork",pid>=0);
	if(pid==0) {
		fpget(&x);if(x!=value) exit(1);
		fpseed(&other);
		args[0]=argv[0];args[1]="exec";args[2]=0;
		execve(argv[0],args,0);exit(2);
	}
	check("fork inherit and exec reset",wait(&status)==pid && status==0);
	fpget(&x);check("parent isolation",x==value);
	signal(SIGALRM,handler);alarm(2);fpseed(&value);
	while(!seen) ;
	fpget(&x);check("signal EPU preservation",x==value && seen==1);
	pid=fork();
	if(pid==0) {
		for(i=0;i<200;i++) if(other/value!= -5.384615384615385) exit(3);
		exit(0);
	}
	for(i=0;i<200;i++) check("preempted arithmetic",value*other== -56.875);
	check("concurrent arithmetic",wait(&status)==pid && status==0);
	pid=fork();if(pid==0) { fpbad();exit(4); }
	check("invalid EPU instruction",wait(&status)==pid && status==(SIGILL<<8));
	pid=fork();if(pid==0) { fpbadmem();exit(4); }
	check("invalid EPU memory",wait(&status)==pid && status==(SIGSEGV<<8));
	pid=fork();if(pid==0) { fptrapdiv();exit(4); }
	check("enabled EPU exception",wait(&status)==pid && status==(SIGFPE<<8));
	printf("fpe: %s\n",failed?"FAILED":"all checks passed");
	return failed!=0;
}
