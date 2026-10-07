#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H
typedef	struct { int r[1]; } *	physadr;
typedef	long		daddr_t;
typedef char *		caddr_t;
typedef	unsigned int	ino_t;
typedef	long		time_t;
typedef	int		label_t[12];	/* Z8000: R4-R7,R10-R12,R14 (8) + caller R13 (FP) + ret addr + caller SP + 1 unused = 12 */
typedef	int		dev_t;
typedef	long		off_t;

/* major part of a device */
#define	major(x)	(int)(((unsigned)x>>8))

/* minor part of a device */
#define	minor(x)	(int)(x&0377)

/* make a device number */
#define	makedev(x,y)	(dev_t)((x)<<8 | (y))


#endif
