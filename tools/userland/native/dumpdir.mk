CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: dumpdir
dumpdir: dumpdir.b
	$(CC) -i -s dumpdir.b -o dumpdir
dumpdir.b: /usr/src/cmd/dumpdir.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/dumpdir.c
install: all
	/bin/cp dumpdir /bin/ninstall
	/bin/mv /bin/ninstall /bin/dumpdir </dev/null
clean:
	/bin/rm -f *.b dumpdir
