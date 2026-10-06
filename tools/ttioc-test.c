/* Appended to the actual ttioccomm definition with target kernel headers. */
int failed, flushes, fault, opens, closes, fallback, masked;
struct tty terminal;
copyin(a,b,n) char *a,*b;
{
	while(n--) { if(fault && n==1) return(-1); *b++ = *a++; }
	return(0);
}
copyout(a,b,n) char *a,*b; { return(copyin(a,b,n)); }
bcopy(a,b,n) char *a,*b; { while(n--) *b++ = *a++; }
wflushtty(tp) struct tty *tp; { flushes++; }
flushtty(tp) struct tty *tp; { flushes++; }
spl5() { masked++;return(7); }
splx(s) { if(s!=7) failed++; masked--; }
lopen(dev,tp,addr) struct tty *tp; char *addr;
{ opens++; }
lclose(tp) struct tty *tp; { closes++; }
lioctl(com,tp,addr) struct tty *tp; char *addr;
{ u.u_error=ENODEV; }
struct linesw linesw[] = {
	{ lopen,lclose,0,0,lioctl,0,0,0,0,0 },
	{ lopen,lclose,0,0,lioctl,0,0,0,0,0 }
};
int nldisp=2;
check(name,ok) char *name;
{ if(!ok) { printf("ttioc: FAIL %s\n",name);failed++; } }
main()
{
	int r, d;
	struct ttiocb params;
	struct tc chars;
	char *p;
	int i;
	/* A driver may handle requests that common code declines. */
	u.u_error=0;
	r=ttioccomm(0,&terminal,0,0);
	if(!r && !u.u_error) fallback++;
	check("driver fallback",r==0 && fallback==1 && u.u_error==0);
	d=1;
	check("alternate open",ttioccomm(TIOCSETD,&terminal,&d,0)==1 &&
	    terminal.t_line==1 && opens==1 && closes==0);
	d=0;
	check("alternate close",ttioccomm(TIOCSETD,&terminal,&d,0)==1 &&
	    terminal.t_line==0 && opens==1 && closes==1);
	check("discipline ioctl recognized",ttioccomm(DIOCGETP,&terminal,0,0)==1 && u.u_error==ENODEV);
	params.ioc_flags=0123;
	params.ioc_erase='#'; params.ioc_kill='@';
	u.u_error=0;
	check("set parameters",ttioccomm(TIOCSETN,&terminal,&params,0)==1 &&
	    terminal.t_flags==0123 && flushes==0 && masked==0);
	fault=1;u.u_error=0;
	check("fault recognized",ttioccomm(TIOCSETP,&terminal,&params,0)==1 && u.u_error==EFAULT);
	check("no flush on copy failure",flushes==0 && terminal.t_flags==0123 && masked==0);
	p=(char *)&chars;for(i=0;i<sizeof(chars);i++) p[i]=i+1;
	fault=0;u.u_error=0;
	check("set special characters",ttioccomm(TIOCSETC,&terminal,&chars,0)==1 && !u.u_error);
	p=(char *)&chars;for(i=0;i<sizeof(chars);i++) p[i]=77;
	fault=1;u.u_error=0;
	check("special character fault",ttioccomm(TIOCSETC,&terminal,&chars,0)==1 && u.u_error==EFAULT);
	p=(char *)&terminal.t_un;
	for(i=0;i<sizeof(chars);i++) check("characters unchanged",p[i]==i+1);
	check("interrupt state",masked==0);
	printf("ttioc: %s\n",failed?"FAILED":"passed");
	return(failed!=0);
}
