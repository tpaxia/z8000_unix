CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: du
du: du.b
	$(CC) -i -s du.b -o du
du.b: /usr/src/cmd/du.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/du.c
install: all
	/bin/cp du /bin/ninstall
	/bin/mv /bin/ninstall /bin/du </dev/null
clean:
	/bin/rm -f *.b du
