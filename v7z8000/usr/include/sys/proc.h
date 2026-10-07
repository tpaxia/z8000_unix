/*
 * One structure allocated per active
 * process. It contains all data needed
 * about the process while the
 * process may be swapped out.
 * Other per process data (user.h)
 * is swapped with the process.
 */
struct	proc {
	char	p_stat;
	char	p_flag;
	char	p_pri;		/* priority, negative is high */
	char	p_time;		/* resident time for scheduling */
	char	p_cpu;		/* cpu usage for scheduling */
	char	p_nice;		/* nice for cpu usage */
	short	p_sig;		/* signals pending to this process */
	short	p_uid;		/* user id, used to direct tty signals */
	short	p_pgrp;		/* name of process group leader */
	short	p_pid;		/* unique process id */
	short	p_ppid;		/* process id of parent */
	short	p_addr;		/* address of swappable image */
	short	p_size;		/* size of swappable image (clicks) */
	caddr_t p_wchan;	/* event process is awaiting */
	struct text *p_textp;	/* pointer to text structure */
	struct proc *p_link;	/* linked list of running processes */
	int	p_clktim;	/* time to alarm clock signal */
};

extern struct proc proc[];	/* the proc table itself */

/* stat codes */
#define	SSLEEP	1		/* awaiting an event */
#define	SWAIT	2		/* (abandoned state) */
#define	SRUN	3		/* running */
#define	SIDL	4		/* intermediate state in process creation */
#define	SZOMB	5		/* intermediate state in process termination */
#define	SSTOP	6		/* process being traced */

/* flag codes */
#define	SLOAD	01		/* in core */
#define	SSYS	02		/* scheduling process */
#define	SLOCK	04		/* process cannot be swapped */
#define	SSWAP	010		/* process is being swapped out */
#define	STRC	020		/* process is being traced */
#define	SWTED	040		/* another tracing flag */
#define	SULOCK	0100		/* user settable lock in core */
#define	SREADY	0200		/* resident image awaiting its first CPU turn */

/*
 * xproc overlay for zombie status collection.
 * In V7, the first part of struct proc is overlaid with
 * exit status and times when the process becomes a zombie.
 */
struct xproc {
	char	xp_stat;
	char	xp_flag;
	char	xp_pri;
	char	xp_time;
	char	xp_cpu;
	char	xp_nice;
	short	xp_sig;
	short	xp_uid;
	short	xp_pgrp;
	short	xp_pid;
	short	xp_ppid;
	short	xp_xstat;	/* overlays p_addr: exit status */
	short	xp_size;
	long	xp_utime;	/* overlays p_wchan + p_textp */
	long	xp_stime;	/* overlays p_link + p_clktim */
};
