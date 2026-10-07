CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: nm
nm: nm.b
	$(CC) -i -s nm.b -o nm
nm.b: /usr/src/cmd/nm.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/nm.c
install: all
	/bin/cp nm /bin/ninstall
	/bin/mv /bin/ninstall /bin/nm </dev/null
clean:
	/bin/rm -f *.b nm
