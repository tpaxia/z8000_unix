CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002
all: libdbm.a
libdbm.a: dbm.b
	/bin/rm -f libdbm.a
	/bin/ar qc libdbm.a dbm.b
dbm.b: /usr/src/libdbm/dbm.c
	$(CC) $(CFLAGS) -c /usr/src/libdbm/dbm.c
install: all
	/bin/cp libdbm.a /lib/libdbm.a
clean:
	/bin/rm -f *.b libdbm.a
