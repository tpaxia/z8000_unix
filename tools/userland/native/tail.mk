CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: tail
tail: tail.b
	$(CC) -i -s tail.b -o tail
tail.b: /usr/src/cmd/tail.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tail.c
install: all
	/bin/cp tail /bin/ninstall
	/bin/mv /bin/ninstall /bin/tail </dev/null
clean:
	/bin/rm -f *.b tail
