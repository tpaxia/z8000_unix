CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: od
od: od.b
	$(CC) -i -s od.b -o od
od.b: /usr/src/cmd/od.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/od.c
install: all
	/bin/cp od /bin/ninstall
	/bin/mv /bin/ninstall /bin/od </dev/null
clean:
	/bin/rm -f *.b od
