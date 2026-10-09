/* Linked with the actual panicflush(), using a polled test block device. */
struct buf buf[NBUF], tab;
struct inode inode[NINODE];
int strategy();
struct bdevsw bdevsw[] = { { 0, 0, strategy, &tab }, { 0 } };
time_t time = 123456L;
char disk[8][BSIZE], data[NBUF][BSIZE];
struct buf *pending;
int mode, polls, releases, failed;
bcopy(src,dst,n)
char *src,*dst;
{ while(n--) *dst++ = *src++; }
strategy(bp)
struct buf *bp;
{
 pending=bp; tab.b_active=1;
}
panicpoll()
{
 struct buf *bp;
 polls++;
 if(mode==2 || !pending)return;
 bp=pending; pending=0; tab.b_active=0;
 if(mode==1)bp->b_flags|=B_ERROR;
 else if(bp->b_flags&B_READ)bcopy(disk[(int)bp->b_blkno],bp->b_un.b_addr,BSIZE);
 else bcopy(bp->b_un.b_addr,disk[(int)bp->b_blkno],BSIZE);
 bp->b_flags|=B_DONE;
}
notavail(bp)
struct buf *bp;
{ bp->b_flags|=B_BUSY; }
brelse(bp)
struct buf *bp;
{ bp->b_flags&=~B_BUSY; releases++; }
check(ok)
{ if(!ok)failed++; }
reset()
{
 int i,j;
 for(i=0;i<NBUF;i++){
  buf[i].b_flags=0;buf[i].b_dev=NODEV;buf[i].b_un.b_addr=data[i];
 }
 for(i=0;i<NINODE;i++)inode[i].i_count=inode[i].i_flag=0;
 for(i=0;i<NMOUNT;i++)mount[i].m_bufp=0;
 for(i=0;i<8;i++)for(j=0;j<BSIZE;j++)disk[i][j]=0;
 mode=polls=releases=0;pending=0;tab.b_active=0;
 pwritten=pskipped=perrors=0;
}
main()
{
 struct filsys fs;
 struct buf super;
 struct dinode *di;
 int i;
 reset();
 buf[0].b_dev=0;buf[0].b_blkno=4;buf[0].b_flags=B_DELWRI;
 data[0][0]=71;
 buf[1].b_dev=0;buf[1].b_blkno=5;buf[1].b_flags=B_DELWRI|B_BUSY;
 data[1][0]=72;
 /* A clean cached inode block must retain both successive inode updates. */
 buf[2].b_dev=0;buf[2].b_blkno=2;buf[2].b_flags=B_DONE;
 for(i=0;i<BSIZE;i++)data[2][i]=0;
 for(i=0;i<sizeof(fs);i++)((char *)&fs)[i]=0;
 fs.s_fmod=1;fs.s_fsize=8;
 /* A mount superblock buffer is BSIZE bytes, as in the real kernel. */
 super.b_un.b_addr=data[3];bcopy((char *)&fs,data[3],sizeof(fs));
 mount[0].m_dev=0;mount[0].m_bufp = &super;
 for(i=0;i<3;i++){
  inode[i].i_count=1;inode[i].i_number=i+1;inode[i].i_dev=0;
  inode[i].i_flag=IUPD|IACC|ICHG|(i==2?ILOCK:0);
  inode[i].i_mode=IFREG|0600;inode[i].i_size=100L+i;
  inode[i].i_un.i_addr[0]=0x123456L+i;
 }
 panicflush();
 check(disk[4][0]==71 && disk[5][0]==0);
 di=(struct dinode *)disk[2];
 check(di[0].di_size==100L && di[1].di_size==101L && di[2].di_size==0);
 check((unsigned char)di[0].di_addr[0]==0x12 &&
       (unsigned char)di[0].di_addr[1]==0x34 &&
       (unsigned char)di[0].di_addr[2]==0x56);
 check(di[0].di_atime==time && di[1].di_ctime==time);
 check(((struct filsys *)disk[1])->s_time==time);
 check(pwritten==4 && pskipped==2 && perrors==0);
 reset();
 /* Drain an already queued transfer before writing a delayed cache block. */
 super.b_flags=B_BUSY;super.b_dev=0;super.b_blkno=6;
 super.b_un.b_addr=data[3];data[3][0]=73;
 strategy(&super);
 buf[0].b_dev=0;buf[0].b_blkno=4;buf[0].b_flags=B_DELWRI;
 data[0][0]=74;
 panicflush();check(disk[6][0]==73 && disk[4][0]==74 && !tab.b_active);
 reset();
 /* Do not read a stale inode block behind its busy cache owner, or commit
  * an allocator's locked superblock. */
 super.b_un.b_addr=data[3];bcopy((char *)&fs,data[3],sizeof(fs));
 ((struct filsys *)data[3])->s_flock=1;
 mount[0].m_dev=0;mount[0].m_bufp = &super;
 buf[0].b_dev=0;buf[0].b_blkno=2;buf[0].b_flags=B_BUSY;
 inode[0].i_dev=0;inode[0].i_number=1;
 inode[0].i_count=1;inode[0].i_flag=IUPD;
 panicflush();check(pskipped==2 && pwritten==0 && polls==0);
 reset();mode=1;
 buf[0].b_dev=0;buf[0].b_blkno=4;buf[0].b_flags=B_DELWRI;
 panicflush();check(perrors==1 && releases==1 && !(buf[0].b_flags&B_BUSY));
 reset();mode=2;
 buf[0].b_dev=0;buf[0].b_blkno=4;buf[0].b_flags=B_DELWRI;
 panicflush();check(polls==30000 && releases==0 && (buf[0].b_flags&B_BUSY));
 reset();mode=2;tab.b_active=1;
 panicflush();check(polls==30000 && pwritten==0);
 printf("panicprobe: %s\n",failed?"FAILED":"passed");
 return(failed);
}
