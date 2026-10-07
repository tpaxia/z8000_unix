CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: ncheck
ncheck: ncheck.b
	$(CC) -i -s ncheck.b -o ncheck
ncheck.b: /usr/src/cmd/ncheck.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/ncheck.c
install: all
	/bin/cp ncheck /bin/ninstall
	/bin/mv /bin/ninstall /bin/ncheck </dev/null
clean:
	/bin/rm -f *.b ncheck
