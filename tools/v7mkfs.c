/*
 * v7mkfs - Host-side V7 filesystem image builder for Z8000
 *
 * Port of V7 mkfs.c for cross-compilation.  Produces big-endian
 * on-disk structures matching the Z8000 target regardless of
 * host byte order.
 *
 * Usage: v7mkfs image proto/size [m n]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

/* ------------------------------------------------------------------ */
/* V7 filesystem constants (from param.h)                             */
/* ------------------------------------------------------------------ */

#define BSIZE   512
#define NICFREE 50
#define NICINOD 100
#define ROOTINO 2
#define DIRSIZ  14
#define NADDR   13
#define INOPB   8       /* 8 inodes per block, each 64 bytes */
#define NDIRECT (BSIZE / 16)    /* directory entries per block */
#define NINDIR  (BSIZE / 4)     /* daddr_t = 4 bytes (long) */
#define LADDR   10
#define MAXFN   500
#define MAXFILEBLKS (LADDR + NINDIR + NINDIR * NINDIR)

/* inode number to disk block */
#define itod(x) ((uint32_t)(((unsigned)(x) + 15) >> 3))
/* inode number to offset within block */
#define itoo(x) ((int)(((x) + 15) & 07))

/* file types and modes */
#define IFMT    0170000
#define IFDIR   0040000
#define IFCHR   0020000
#define IFBLK   0060000
#define IFREG   0100000
#define ISUID   04000
#define ISGID   02000

/* ------------------------------------------------------------------ */
/* Big-endian byte-order helpers                                      */
/* ------------------------------------------------------------------ */

static void put16(uint8_t *p, uint16_t v)
{
	p[0] = (v >> 8) & 0xff;
	p[1] = v & 0xff;
}

static void put32(uint8_t *p, uint32_t v)
{
	p[0] = (v >> 24) & 0xff;
	p[1] = (v >> 16) & 0xff;
	p[2] = (v >> 8) & 0xff;
	p[3] = v & 0xff;
}

static uint16_t get16(const uint8_t *p)
{
	return ((uint16_t)p[0] << 8) | p[1];
}

static uint32_t get32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	       ((uint32_t)p[2] << 8) | p[3];
}

/*
 * Pack an array of longs into 3-byte big-endian disk addresses.
 * On the Z8000 (big-endian) the 3-byte format stores the high
 * byte first: bits 23..16, 15..8, 7..0.
 */
static void ltol3(uint8_t *cp, uint32_t *lp, int n)
{
	int i;
	for (i = 0; i < n; i++) {
		uint32_t v = lp[i];
		*cp++ = (v >> 16) & 0xff;
		*cp++ = (v >> 8) & 0xff;
		*cp++ = v & 0xff;
	}
}

/* ------------------------------------------------------------------ */
/* On-disk structure sizes (fixed, independent of host)               */
/*                                                                    */
/* We do NOT define C structs for on-disk layout because their size   */
/* and alignment depend on the host compiler.  Instead we serialise   */
/* every field through the byte-order helpers into flat uint8_t       */
/* buffers at known offsets.                                          */
/* ------------------------------------------------------------------ */

/*
 * struct dinode (64 bytes on disk):
 *   0  uint16  di_mode
 *   2  int16   di_nlink
 *   4  int16   di_uid
 *   6  int16   di_gid
 *   8  int32   di_size   (off_t)
 *  12  char[40] di_addr  (13 x 3-byte addresses = 39 used, 40 stored)
 *  52  int32   di_atime
 *  56  int32   di_mtime
 *  60  int32   di_ctime
 */
#define DINODE_SZ   64
#define DI_MODE     0
#define DI_NLINK    2
#define DI_UID      4
#define DI_GID      6
#define DI_SIZE     8
#define DI_ADDR     12
#define DI_ATIME    52
#define DI_MTIME    56
#define DI_CTIME    60

/*
 * struct direct (16 bytes on disk):
 *   0  uint16  d_ino
 *   2  char[14] d_name
 */
#define DIRECT_SZ   16
#define DD_INO      0
#define DD_NAME     2

/*
 * Superblock layout (struct filsys, block 1, 512 bytes):
 *   0   uint16         s_isize
 *   2   int32          s_fsize      (daddr_t)
 *   6   int16          s_nfree
 *   8   int32[50]      s_free       (daddr_t[NICFREE])
 * 208   int16          s_ninode
 * 210   uint16[100]    s_inode      (ino_t[NICINOD])
 * 410   char           s_flock
 * 411   char           s_ilock
 * 412   char           s_fmod
 * 413   char           s_ronly
 * 414   int32          s_time       (time_t)
 * 418   int32          s_tfree      (daddr_t)
 * 422   uint16         s_tinode     (ino_t)
 * 424   int16          s_m
 * 426   int16          s_n
 * 428   char[6]        s_fname
 * 434   char[6]        s_fpack
 */
#define SB_ISIZE    0
#define SB_FSIZE    2
#define SB_NFREE    6
#define SB_FREE     8       /* 50 * 4 = 200 bytes */
#define SB_NINODE   208
#define SB_INODE    210     /* 100 * 2 = 200 bytes */
#define SB_FLOCK    410
#define SB_ILOCK    411
#define SB_FMOD     412
#define SB_RONLY    413
#define SB_TIME     414
#define SB_TFREE    418
#define SB_TINODE   422
#define SB_M        424
#define SB_N        426
#define SB_FNAME    428
#define SB_FPACK    434

/*
 * Free-block list block (struct fblk):
 *   0   int16          df_nfree     (actually int = 16-bit on Z8000)
 *   2   int32[50]      df_free      (daddr_t[NICFREE])
 */
#define FB_NFREE    0
#define FB_FREE     2

/* ------------------------------------------------------------------ */
/* In-memory working copies (host-native byte order)                  */
/* ------------------------------------------------------------------ */

/* In-memory superblock */
static struct {
	uint16_t s_isize;
	uint32_t s_fsize;
	int16_t  s_nfree;
	uint32_t s_free[NICFREE];
	int16_t  s_ninode;
	uint16_t s_inode[NICINOD];
	uint32_t s_time;
	uint32_t s_tfree;
	uint16_t s_tinode;
	int16_t  s_m;
	int16_t  s_n;
} filsys;

/* In-memory free-block buffer */
static struct {
	int16_t  df_nfree;
	uint32_t df_free[NICFREE];
} fbuf;

/* In-memory inode (matches V7 in-core inode fields we need) */
struct inode {
	uint16_t i_number;
	uint16_t i_mode;
	int16_t  i_nlink;
	int16_t  i_uid;
	int16_t  i_gid;
	uint32_t i_size;
	uint32_t i_addr[NADDR];
};

/* ------------------------------------------------------------------ */
/* Globals                                                            */
/* ------------------------------------------------------------------ */

static uint32_t utime_val;
static FILE *fin;
static int fsi;
static int fso;
static char *charp;
static char buf[BSIZE];
static char string[4096];
static char *proto;
static int f_n = MAXFN;
static int f_m = 3;
static int error;
static uint16_t ino;

/* Forward declarations */
static long getnum(void);
static uint32_t alloc_blk(void);
static void cfile(struct inode *par);
static void bflist(void);
static void bfree(uint32_t bno);
static void iput(struct inode *ip, int *aibc, uint32_t *ib);
static void entry(uint16_t inum, const char *str, int *adbc,
                  char *db, int *aibc, uint32_t *ib);
static void newblk(int *adbc, char *db, int *aibc, uint32_t *ib);
static int gmode(int c, const char *s, int m0, int m1, int m2, int m3);
static void getstr(void);
static int getch(void);

/* ------------------------------------------------------------------ */
/* Serialise superblock to a 512-byte on-disk block                   */
/* ------------------------------------------------------------------ */

static void sb_to_disk(uint8_t *blk)
{
	int i;
	memset(blk, 0, BSIZE);
	put16(blk + SB_ISIZE,  filsys.s_isize);
	put32(blk + SB_FSIZE,  filsys.s_fsize);
	put16(blk + SB_NFREE,  (uint16_t)filsys.s_nfree);
	for (i = 0; i < NICFREE; i++)
		put32(blk + SB_FREE + i * 4, filsys.s_free[i]);
	put16(blk + SB_NINODE, (uint16_t)filsys.s_ninode);
	for (i = 0; i < NICINOD; i++)
		put16(blk + SB_INODE + i * 2, filsys.s_inode[i]);
	put32(blk + SB_TIME,   filsys.s_time);
	put32(blk + SB_TFREE,  filsys.s_tfree);
	put16(blk + SB_TINODE, filsys.s_tinode);
	put16(blk + SB_M,      (uint16_t)filsys.s_m);
	put16(blk + SB_N,      (uint16_t)filsys.s_n);
}

/* ------------------------------------------------------------------ */
/* Serialise free-block list to a 512-byte on-disk block              */
/* ------------------------------------------------------------------ */

static void fb_to_disk(uint8_t *blk)
{
	int i;
	memset(blk, 0, BSIZE);
	put16(blk + FB_NFREE, (uint16_t)fbuf.df_nfree);
	for (i = 0; i < NICFREE; i++)
		put32(blk + FB_FREE + i * 4, fbuf.df_free[i]);
}

/* ------------------------------------------------------------------ */
/* Deserialise free-block list from a 512-byte on-disk block          */
/* ------------------------------------------------------------------ */

static void fb_from_disk(const uint8_t *blk)
{
	int i;
	fbuf.df_nfree = (int16_t)get16(blk + FB_NFREE);
	for (i = 0; i < NICFREE; i++)
		fbuf.df_free[i] = get32(blk + FB_FREE + i * 4);
}

/* ------------------------------------------------------------------ */
/* Disk I/O                                                           */
/* ------------------------------------------------------------------ */

static void rdfs(uint32_t bno, char *bf)
{
	if (lseek(fsi, (off_t)bno * BSIZE, SEEK_SET) == -1) {
		printf("seek error: %u\n", bno);
		exit(1);
	}
	if (read(fsi, bf, BSIZE) != BSIZE) {
		printf("read error: %u\n", bno);
		exit(1);
	}
}

static void wtfs(uint32_t bno, const char *bf)
{
	if (lseek(fso, (off_t)bno * BSIZE, SEEK_SET) == -1) {
		printf("seek error: %u\n", bno);
		exit(1);
	}
	if (write(fso, bf, BSIZE) != BSIZE) {
		printf("write error: %u\n", bno);
		exit(1);
	}
}

/* ------------------------------------------------------------------ */
/* Block allocator                                                    */
/* ------------------------------------------------------------------ */

static uint32_t alloc_blk(void)
{
	int i;
	uint32_t bno;

	filsys.s_tfree--;
	bno = filsys.s_free[--filsys.s_nfree];
	if (bno == 0) {
		printf("out of free space\n");
		exit(1);
	}
	if (filsys.s_nfree <= 0) {
		char tmp[BSIZE];
		rdfs(bno, tmp);
		fb_from_disk((uint8_t *)tmp);
		filsys.s_nfree = fbuf.df_nfree;
		for (i = 0; i < NICFREE; i++)
			filsys.s_free[i] = fbuf.df_free[i];
	}
	return bno;
}

static void bfree(uint32_t bno)
{
	int i;

	filsys.s_tfree++;
	if (filsys.s_nfree >= NICFREE) {
		fbuf.df_nfree = filsys.s_nfree;
		for (i = 0; i < NICFREE; i++)
			fbuf.df_free[i] = filsys.s_free[i];
		{
			char tmp[BSIZE];
			fb_to_disk((uint8_t *)tmp);
			wtfs(bno, tmp);
		}
		filsys.s_nfree = 0;
	}
	filsys.s_free[filsys.s_nfree++] = bno;
}

/* ------------------------------------------------------------------ */
/* Proto-file parser helpers                                          */
/* ------------------------------------------------------------------ */

static int getch(void)
{
	if (charp)
		return *charp++;
	return fgetc(fin);
}

static void getstr(void)
{
	int i, c;

loop:
	switch (c = getch()) {
	case ' ':
	case '\t':
	case '\n':
		goto loop;
	case EOF:
	case '\0':
		printf("EOF\n");
		exit(1);
	case ':':
		while (getch() != '\n')
			;
		goto loop;
	}
	i = 0;
	do {
		if (i >= (int)sizeof(string) - 1) {
			fprintf(stderr, "prototype token too long\n");
			exit(1);
		}
		string[i++] = c;
		c = getch();
	} while (c != ' ' && c != '\t' && c != '\n' && c != '\0' && c != EOF);
	string[i] = '\0';
}

static long getnum(void)
{
	int i, c;
	long n;

	getstr();
	n = 0;
	for (i = 0; (c = string[i]) != '\0'; i++) {
		if (c < '0' || c > '9') {
			printf("%s: bad number\n", string);
			error = 1;
			return 0;
		}
		n = n * 10 + (c - '0');
	}
	return n;
}

static int gmode(int c, const char *s, int m0, int m1, int m2, int m3)
{
	int modes[4];
	int i;

	modes[0] = m0;
	modes[1] = m1;
	modes[2] = m2;
	modes[3] = m3;
	for (i = 0; s[i]; i++)
		if (c == s[i])
			return modes[i];
	printf("%c/%s: bad mode\n", c, string);
	error = 1;
	return 0;
}

/* ------------------------------------------------------------------ */
/* Directory entry helper                                             */
/* ------------------------------------------------------------------ */

static void entry(uint16_t inum, const char *str, int *adbc,
                  char *db, int *aibc, uint32_t *ib)
{
	uint8_t *dp;
	int i;

	dp = (uint8_t *)db + (*adbc) * DIRECT_SZ;
	(*adbc)++;
	put16(dp + DD_INO, inum);
	memset(dp + DD_NAME, 0, DIRSIZ);
	for (i = 0; i < DIRSIZ && str[i] != '\0'; i++)
		dp[DD_NAME + i] = str[i];
	if (*adbc >= NDIRECT)
		newblk(adbc, db, aibc, ib);
}

/* ------------------------------------------------------------------ */
/* Allocate a new data block and record it                            */
/* ------------------------------------------------------------------ */

static void newblk(int *adbc, char *db, int *aibc, uint32_t *ib)
{
	int i;
	uint32_t bno;

	if (*aibc >= MAXFILEBLKS) {
		fprintf(stderr, "file exceeds double-indirect capacity\n");
		exit(1);
	}
	bno = alloc_blk();
	wtfs(bno, db);
	for (i = 0; i < BSIZE; i++)
		db[i] = 0;
	*adbc = 0;
	ib[*aibc] = bno;
	(*aibc)++;
}

/* ------------------------------------------------------------------ */
/* Write inode to disk                                                */
/* ------------------------------------------------------------------ */

static void iput(struct inode *ip, int *aibc, uint32_t *ib)
{
	uint8_t *dp;
	uint32_t d;
	int i;

	filsys.s_tinode--;
	d = itod(ip->i_number);
	if (d >= filsys.s_isize) {
		if (error == 0)
			printf("ilist too small\n");
		error = 1;
		return;
	}
	rdfs(d, buf);
	dp = (uint8_t *)buf + itoo(ip->i_number) * DINODE_SZ;

	put16(dp + DI_MODE,  ip->i_mode);
	put16(dp + DI_NLINK, (uint16_t)ip->i_nlink);
	put16(dp + DI_UID,   (uint16_t)ip->i_uid);
	put16(dp + DI_GID,   (uint16_t)ip->i_gid);
	put32(dp + DI_SIZE,  ip->i_size);
	put32(dp + DI_ATIME, utime_val);
	put32(dp + DI_MTIME, utime_val);
	put32(dp + DI_CTIME, utime_val);

	switch (ip->i_mode & IFMT) {
	case IFDIR:
	case IFREG:
		for (i = 0; i < *aibc && i < LADDR; i++)
			ip->i_addr[i] = ib[i];
		if (*aibc > LADDR) {
			char block[BSIZE];
			int j;
			memset(block, 0, sizeof(block));
			for (j = 0; j < NINDIR && LADDR+j < *aibc; j++)
				put32((uint8_t *)block+j*4, ib[LADDR+j]);
			ip->i_addr[LADDR] = alloc_blk();
			wtfs(ip->i_addr[LADDR], block);
		}
		if (*aibc > LADDR + NINDIR) {
			char outer[BSIZE], inner[BSIZE];
			int j, k, offset = LADDR + NINDIR;
			memset(outer, 0, sizeof(outer));
			for (j = 0; offset < *aibc; j++) {
				uint32_t block = alloc_blk();
				memset(inner, 0, sizeof(inner));
				for (k = 0; k < NINDIR && offset < *aibc; k++, offset++)
					put32((uint8_t *)inner+k*4, ib[offset]);
				wtfs(block, inner);
				put32((uint8_t *)outer+j*4, block);
			}
			ip->i_addr[LADDR+1] = alloc_blk();
			wtfs(ip->i_addr[LADDR+1], outer);
		}
		/* fall through */
	case IFBLK:
	case IFCHR:
		ltol3(dp + DI_ADDR, ip->i_addr, NADDR);
		break;
	default:
		printf("bad mode %o\n", ip->i_mode);
		exit(1);
	}
	wtfs(d, buf);
}

/* ------------------------------------------------------------------ */
/* Build the free block list                                          */
/* ------------------------------------------------------------------ */

static void bflist(void)
{
	struct inode in;
	uint32_t ib[MAXFILEBLKS];
	int ibc;
	char flg[MAXFN];
	int adr[MAXFN];
	int i, j;
	uint32_t f, d;

	for (i = 0; i < f_n; i++)
		flg[i] = 0;
	i = 0;
	for (j = 0; j < f_n; j++) {
		while (flg[i])
			i = (i + 1) % f_n;
		adr[j] = i + 1;
		flg[i]++;
		i = (i + f_m) % f_n;
	}

	ino++;
	memset(&in, 0, sizeof(in));
	in.i_number = ino;
	in.i_mode = IFREG;

	for (i = 0; i < MAXFILEBLKS; i++)
		ib[i] = 0;
	ibc = 0;
	bfree(0);
	d = filsys.s_fsize - 1;
	while (d % f_n)
		d++;
	for (; d > 0; d -= f_n)
		for (i = 0; i < f_n; i++) {
			f = d - adr[i];
			if (f < filsys.s_fsize && f >= filsys.s_isize)
				bfree(f);
		}
	iput(&in, &ibc, ib);
}

/* ------------------------------------------------------------------ */
/* Recursive proto-file processor                                     */
/* ------------------------------------------------------------------ */

static void cfile(struct inode *par)
{
	struct inode in;
	int dbc, ibc;
	char db[BSIZE];
	uint32_t ib[MAXFILEBLKS];
	int i, f, c;

	/* get mode, uid and gid */
	getstr();
	in.i_mode = gmode(string[0], "-bcd", IFREG, IFBLK, IFCHR, IFDIR);
	in.i_mode |= gmode(string[1], "-u", 0, ISUID, 0, 0);
	in.i_mode |= gmode(string[2], "-g", 0, ISGID, 0, 0);
	for (i = 3; i < 6; i++) {
		c = string[i];
		if (c < '0' || c > '7') {
			printf("%c/%s: bad octal mode digit\n", c, string);
			error = 1;
			c = '0';
		}
		in.i_mode |= (c - '0') << (15 - 3 * i);
	}
	in.i_uid = (int16_t)getnum();
	in.i_gid = (int16_t)getnum();

	/* general initialization */
	ino++;
	in.i_number = ino;
	memset(db, 0, BSIZE);
	for (i = 0; i < MAXFILEBLKS; i++)
		ib[i] = 0;
	in.i_nlink = 1;
	in.i_size = 0;
	for (i = 0; i < NADDR; i++)
		in.i_addr[i] = 0;
	if (par == NULL) {
		par = &in;
		in.i_nlink--;
	}
	dbc = 0;
	ibc = 0;

	switch (in.i_mode & IFMT) {
	case IFREG:
		/* regular file: contents is a file name */
		getstr();
		f = open(string, O_RDONLY);
		if (f < 0) {
			printf("%s: cannot open\n", string);
			error = 1;
			break;
		}
		while ((i = read(f, db, BSIZE)) > 0) {
			in.i_size += i;
			newblk(&dbc, db, &ibc, ib);
		}
		close(f);
		break;

	case IFBLK:
	case IFCHR:
		/* special file: content is maj/min types */
		i = getnum() & 0377;
		f = getnum() & 0377;
		in.i_addr[0] = (i << 8) | f;
		break;

	case IFDIR:
		/* directory */
		par->i_nlink++;
		in.i_nlink++;
		entry(in.i_number, ".", &dbc, db, &ibc, ib);
		entry(par->i_number, "..", &dbc, db, &ibc, ib);
		in.i_size = 2 * DIRECT_SZ;
		for (;;) {
			getstr();
			if (string[0] == '$' && string[1] == '\0')
				break;
			entry(ino + 1, string, &dbc, db, &ibc, ib);
			in.i_size += DIRECT_SZ;
			cfile(&in);
		}
		break;
	}
	if (dbc != 0)
		newblk(&dbc, db, &ibc, ib);
	iput(&in, &ibc, ib);
}

/* ------------------------------------------------------------------ */
/* Main                                                               */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
	int c;
	long n;
	uint8_t sbblk[BSIZE];

	utime_val = (uint32_t)time(NULL);

	if (argc < 3) {
		printf("usage: v7mkfs image proto/size [m n]\n");
		exit(1);
	}
	proto = argv[2];

	fso = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fso < 0) {
		printf("%s: cannot create\n", argv[1]);
		exit(1);
	}
	fsi = open(argv[1], O_RDWR);
	if (fsi < 0) {
		printf("%s: cannot open\n", argv[1]);
		exit(1);
	}
	fin = fopen(proto, "r");

	if (fin == NULL) {
		/* proto is a numeric size */
		n = 0;
		for (c = 0; proto[c]; c++) {
			if (proto[c] < '0' || proto[c] > '9') {
				printf("%s: cannot open\n", proto);
				exit(1);
			}
			n = n * 10 + (proto[c] - '0');
		}
		filsys.s_fsize = n;
		n = n / 25;
		if (n <= 0)
			n = 1;
		if (n > 65500 / INOPB)
			n = 65500 / INOPB;
		filsys.s_isize = n + 2;
		printf("isize = %ld\n", n * INOPB);
		charp = "d--777 0 0 $ ";
		goto f3;
	}

	/*
	 * Read proto file: skip boot program name,
	 * get total size and inode count
	 */
	getstr();	/* boot program name (ignored) */
	filsys.s_fsize = getnum();
	n = getnum();
	n /= INOPB;
	filsys.s_isize = n + 3;

f3:
	if (argc >= 5) {
		f_m = atoi(argv[3]);
		f_n = atoi(argv[4]);
		if (f_n <= 0 || f_n >= MAXFN)
			f_n = MAXFN;
		if (f_m <= 0 || f_m > f_n)
			f_m = 3;
	}
	filsys.s_m = f_m;
	filsys.s_n = f_n;
	printf("m/n = %d %d\n", f_m, f_n);
	if (filsys.s_isize >= filsys.s_fsize) {
		printf("%u/%u: bad ratio\n", filsys.s_fsize, filsys.s_isize - 2);
		exit(1);
	}
	filsys.s_tfree = 0;
	filsys.s_tinode = 0;

	/* zero out inode blocks */
	memset(buf, 0, BSIZE);
	for (n = 2; n != (long)filsys.s_isize; n++) {
		wtfs(n, buf);
		filsys.s_tinode += INOPB;
	}
	ino = 0;

	bflist();

	cfile(NULL);

	filsys.s_time = utime_val;
	sb_to_disk(sbblk);
	wtfs(1, (char *)sbblk);

	close(fsi);
	close(fso);
	if (fin)
		fclose(fin);
	exit(error);
}
