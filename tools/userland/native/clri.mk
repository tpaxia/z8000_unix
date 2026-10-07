CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: clri
clri: clri.b
	$(CC) -i -s clri.b -o clri
clri.b: /usr/src/cmd/clri.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/clri.c
install: all
	/bin/cp clri /bin/ninstall
	/bin/mv /bin/ninstall /bin/clri </dev/null
clean:
	/bin/rm -f *.b clri
