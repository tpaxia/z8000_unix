#
/*
 *	UNIX/Z8000 debugger
 */

/* unix parameters */
#define DBNAME "adb\n"
#define LPRMODE "%Q"
#define OFFMODE "+%o"
#define TXTRNDSIZ 8192L


TYPE	unsigned SYMV;

/* In-memory symbol; the shared s.out reader decodes the disk record. */
struct symtab {
	char	symc[8];
	int	symf;
	SYMV	symv;
};
#define SYMTABSIZ 14

#define SYMCHK 047
#define SYMTYPE(symflg) (( symflg>=041 || (symflg>=02 && symflg<=04))\
				?  ((symflg&07)>=3 ? DSYM : (symflg&07))\
				: NSYM\
			)
