#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
extern int errno;
long lseek();
int failed;
check(name,ok) char *name;
{ if(!ok) { printf("v7fs: FAIL %s errno=%d\n",name,errno);failed++; } }
main()
{
	int fd, other, p[2], pid, status;
	char c, buf[4];
	struct stat st;
	check("large directory",stat("/large",&st)==0 && st.st_size>65536L);
	fd=open("/large/n4099",0);
	check("lookup past 64K",fd>=0); if(fd>=0) close(fd);
	fd=creat("/large/last!file",0600);
	check("create past 64K",fd>=0 && write(fd,"ABCD",4)==4); close(fd);
	check("link literal exclamation",link("/large/last!file","/tmp/link!file")==0);
	check("unlink original",unlink("/large/last!file")==0);
	fd=open("/tmp/link!file",0); other=dup(fd);
	check("dup shared offset",fd>=0 && other>=0 && read(fd,&c,1)==1 && c=='A' &&
	    read(other,&c,1)==1 && c=='B' && lseek(fd,0L,1)==2L);
	close(fd);
	check("close one reference",read(other,buf,2)==2 && buf[0]=='C' && buf[1]=='D');
	close(other);
	check("unlink remaining",unlink("/tmp/link!file")==0);
	check("ordinary exclamation lookup",open("/large/n4099!",0)==-1 && errno==ENOENT);
	check("pipe",pipe(p)==0);
	check("pipe seek rejected",lseek(p[0],0L,0)==-1L && errno==ESPIPE);
	pid=fork();
	if(pid==0) { close(p[0]); _exit(write(p[1],"pipe",4)!=4); }
	close(p[1]);
	check("pipe data",read(p[0],buf,4)==4 && buf[0]=='p' && buf[3]=='e');
	check("pipe eof",read(p[0],&c,1)==0); close(p[0]);
	check("child exit",pid>0 && wait(&status)==pid && status==0);
	check("multiplexor disabled",mpxprobe()==0);
	printf("v7fs: %s\n",failed?"FAILED":"passed");
	return(failed!=0);
}
