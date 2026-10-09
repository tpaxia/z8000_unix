/* Board image preparation and boot-block installation. Runs under V7 too.
 * No compiler or instruction encoding lives here; all input images are linked
 * by ldz8. Explicit byte order keeps disk/image contracts host independent.
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

unsigned char buf[512], head[40], inode[64];
unsigned short word(p) unsigned char *p; { return (p[0]<<8)|p[1]; }
long quad(p) unsigned char *p;
{ return ((long)p[0]<<24)|((long)p[1]<<16)|((long)p[2]<<8)|p[3]; }
putword(p,n) unsigned char *p; unsigned n; { p[0]=n>>8; p[1]=n; }
putquad(p,n) unsigned char *p; long n;
{ p[0]=n>>24; p[1]=n>>16; p[2]=n>>8; p[3]=n; }
fail(s) char *s; { fprintf(stderr,"pack: %s\n",s); exit(1); }
FILE *input(s) char *s; { FILE *f; f=fopen(s,"r"); if(!f)fail(s); return f; }
FILE *output(s) char *s; { FILE *f; f=fopen(s,"w"); if(!f)fail(s); return f; }
finish(f) FILE *f; { if(ferror(f)||fclose(f))fail("file I/O"); }
copy(f,g,n) FILE *f,*g; long n;
{
    int k;
    while(n) { k=n>512?512:n; if(fread(buf,1,k,f)!=k)fail("short image");
        if(fwrite(buf,1,k,g)!=k)fail("write image"); n-=k; }
}
textcopy(f,g) FILE *f,*g;
{ while(fgets((char *)buf,sizeof buf,f))fputs((char *)buf,g); if(ferror(f))fail("read source"); }
cut(kind,src,dst) char *kind,*src,*dst;
{
    FILE *f,*g; int skipping,found;
    f=input(src); g=output(dst); skipping=found=0;
    while(fgets((char *)buf,sizeof buf,f)) {
        if(!strcmp(kind,"prfcut")&&!strncmp((char *)buf,"struct\tdevice",13)) { found=1; break; }
        if(!strcmp(kind,"fpecut")) {
            if(!strncmp((char *)buf,"frm_set\t.equ",12)) { skipping=1; found=1; }
            if(skipping && !strcmp((char *)buf,"epu:\n")) { skipping=0; fputs("\n.text\n",g); }
        }
        if(!skipping)fputs((char *)buf,g);
    }
    if(!found||skipping||ferror(f))fail("source boundary");
    finish(f); finish(g);
}
handoff(board,machine,dst) char *board,*machine,*dst;
{
    FILE *f,*g; int found;
    g=output(dst); f=input(board); textcopy(f,g); finish(f);
    fputs("\n.org 0x200\n",g); f=input(machine); found=0;
    while(fgets((char *)buf,sizeof buf,f))if(!strncmp((char *)buf,"initboot:",9)) {found=1;break;}
    if(!found)fail("missing kernel handoff"); textcopy(f,g); finish(f); finish(g);
}
pad(src,dst,n,value) char *src,*dst; long n; int value;
{
    FILE *f,*g; int c;
    f=input(src); g=output(dst);
    while((c=getc(f))!=EOF) { if(n--<=0)fail("image too large"); putc(c,g); }
    while(n-->0)putc(value,g); finish(f); finish(g);
}
kernel(src,trap,dst) char *src,*trap,*dst;
{
    FILE *f,*g,*t; long text,data,bss; int n,c;
    f=input(src); t=input(trap);
    if(fread(head,1,40,f)!=40)fail("kernel header");
    text=word(head+28); data=word(head+30); bss=word(head+32);
    if(word(head)!=0xe711||word(head+10)!=16||word(head+18)!=1||
       text<524||data+bss>0xe000L||quad(head+2)!=text+data||quad(head+6)!=bss)
        fail("kernel layout");
    if(fread(buf,1,512,f)!=512)fail("kernel vectors");
    for(n=0;n<512;n++)if(buf[n])fail("kernel vector reservation");
    for(n=0;n<512;n++)buf[n]=0;
    n=fread(buf,1,512,t); c=getc(t);
    if(n==0||c!=EOF||ferror(t))fail("trap size"); finish(t);
    putquad(head+14,0x1f0L);
    g=output(dst); fwrite(head,1,40,g); fwrite(buf,1,512,g);
    copy(f,g,text+data-512+word(head+12)); finish(f); finish(g);
}
sector(fd,n) int fd; long n;
{ if(lseek(fd,n*512L,0)<0||read(fd,buf,512)!=512)fail("read disk sector"); }
install(block,boot,device) char *block,*boot,*device;
{
    struct stat st,dev; FILE *f; int fd,n,i; long size,isect,addr,max,first;
    unsigned char image[512],indirect[512];
    if(stat(boot,&st)||stat(device,&dev)||(st.st_mode&0170000)!=0100000||
       (dev.st_mode&0170000)!=060000||st.st_dev!=dev.st_rdev)
        fail("boot and block device must be on the same filesystem");
    size=st.st_size; if(size<40||size>63L*512L)fail("boot exceeds sector list");
    n=(size+511L)/512L;
    if(n<1||n>63)fail("boot exceeds sector list");
    f=input(boot); if(fread(head,1,40,f)!=40)fail("boot header");finish(f);
    if(word(head)!=0xe707||word(head+10)!=16||word(head+12)||quad(head+14)||word(head+18)!=1||
       quad(head+2)+40!=size||quad(head+2)!=(long)word(head+28)+word(head+30)||
       (long)word(head+28)+word(head+30)+word(head+32)>0xe000L)
        fail("boot layout");
    f=input(block); if(fread(image,1,512,f)!=512||getc(f)!=EOF||word(image)!=0x5a39)
        fail("boot block layout");finish(f);
    sync(); fd=open(device,2); if(fd<0)fail(device);
    sector(fd,1L); first=word(buf); max=quad(buf+2);
    isect=((long)st.st_ino+15)/8;
    if(first<3||first>=max||isect<2||isect>=first)fail("filesystem inode bounds");
    sector(fd,isect); i=((st.st_ino+15)%8)*64;
    for(n=0;n<64;n++)inode[n]=buf[i+n];
    if(quad(inode+8)!=size)fail("boot inode changed");
    n=(size+511L)/512L; putword(image+256,n);
    if(n>10) {
        addr=((long)inode[42]<<16)|((long)inode[43]<<8)|inode[44];
        if(addr<first||addr>=max)fail("boot indirect block"); sector(fd,addr);
        for(i=0;i<512;i++)indirect[i]=buf[i];
    }
    for(i=0;i<n;i++) {
        if(i<10)addr=((long)inode[12+i*3]<<16)|((long)inode[13+i*3]<<8)|inode[14+i*3];
        else addr=quad(indirect+4*(i-10));
        if(addr<first||addr>=max)fail("boot data block"); putquad(image+258+4*i,addr);
    }
    if(lseek(fd,0L,0)<0||write(fd,image,512)!=512||close(fd))fail("write boot sector");
    sync(); printf("Installed %d boot sectors on %s\n",n,device);
}
main(argc,argv) int argc; char **argv;
{
    long atol();
    if(argc==5&&!strcmp(argv[1],"kernel"))kernel(argv[2],argv[3],argv[4]);
    else if(argc==5&&!strcmp(argv[1],"handoff"))handoff(argv[2],argv[3],argv[4]);
    else if(argc==4&&(!strcmp(argv[1],"prfcut")||!strcmp(argv[1],"fpecut")))cut(argv[1],argv[2],argv[3]);
    else if(argc==6&&!strcmp(argv[1],"pad"))pad(argv[2],argv[3],atol(argv[4]),atoi(argv[5]));
    else if(argc==5&&!strcmp(argv[1],"install"))install(argv[2],argv[3],argv[4]);
    else fail("usage: pack kernel|handoff|install in in out; pack prfcut|fpecut in out; pack pad in out size byte");
    return 0;
}
