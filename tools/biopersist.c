#include <stdio.h>
long lseek();
char block[512];
main(argc,argv) int argc;char **argv;
{
	int fd,i,j,writing;
	if(argc!=2) return(1);
	writing=argv[1][0]=='w';
	fd=writing?creat("/tmp/cachedata",0600):open("/tmp/cachedata",0);
	if(fd<0) return(2);
	for(i=0;i<24;i++) {
		if(writing) {
			for(j=0;j<512;j++) block[j]=(i*17+j)%251;
			if(write(fd,block,512)!=512) return(3);
		} else {
			if(read(fd,block,512)!=512) return(4);
			for(j=0;j<512;j++) if((block[j]&255)!=(j==37?99:(i*17+j)%251)) return(5);
		}
	}
	/* Many partial blocks dirty more buffers than the cache can hold. */
	for(i=0;i<24;i++) {
		if(lseek(fd,i*512L+37,0)!=i*512L+37) return(6);
		if(writing) { block[0]=99;if(write(fd,block,1)!=1) return(7); }
	}
	close(fd);if(writing) sync();
	printf("persist: %s\n",writing?"written":"verified");return(0);
}
