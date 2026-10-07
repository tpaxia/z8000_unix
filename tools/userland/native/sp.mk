CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: sp
sp: sp.b
	$(CC) -i -s sp.b -o sp
sp.b: /usr/src/cmd/sp.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/sp.c
install: all
	/bin/cp sp /bin/ninstall
	/bin/mv /bin/ninstall /bin/sp </dev/null
clean:
	/bin/rm -f *.b sp
