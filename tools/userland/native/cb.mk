CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: cb
cb: cb.b
	$(CC) -i -s cb.b -o cb
cb.b: /usr/src/cmd/cb.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/cb.c
install: all
	/bin/cp cb /bin/ninstall
	/bin/mv /bin/ninstall /bin/cb </dev/null
clean:
	/bin/rm -f *.b cb
