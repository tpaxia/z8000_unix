CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: rev
rev: rev.b
	$(CC) -i -s rev.b -o rev
rev.b: /usr/src/cmd/rev.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/rev.c
install: all
	/bin/cp rev /bin/ninstall
	/bin/mv /bin/ninstall /bin/rev </dev/null
clean:
	/bin/rm -f *.b rev
