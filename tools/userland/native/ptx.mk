CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: ptx
ptx: ptx.b
	$(CC) -i -s ptx.b -o ptx
ptx.b: /usr/src/cmd/ptx.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/ptx.c
install: all
	/bin/cp ptx /bin/ninstall
	/bin/mv /bin/ninstall /bin/ptx </dev/null
clean:
	/bin/rm -f *.b ptx
