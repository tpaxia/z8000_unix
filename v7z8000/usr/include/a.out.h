/* V7 nlist API; objects and executables use Z8000 s.out. */
struct	nlist {	/* symbol table entry */
	char    	n_name[8];	/* symbol name */
	int     	n_type;    	/* type flag */
	unsigned	n_value;	/* value */
};

		/* values for type flag */
#define	N_UNDF	0	/* undefined */
#define	N_ABS	01	/* absolute */
#define	N_TEXT	02	/* text symbol */
#define	N_DATA	03	/* data symbol */
#define	N_BSS	04	/* bss symbol */
#define	N_TYPE	037
#define	N_REG	024	/* register name */
#define	N_FN	037	/* file name symbol */
#define	N_EXT	040	/* external bit, or'ed in */
#define	FORMAT	"%06o"	/* to print a value */
