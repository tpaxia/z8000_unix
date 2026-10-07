CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: cu
cu: cu.b
	$(CC) -i -s cu.b -o cu
cu.b: /usr/src/cmd/cu.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/cu.c
install: all
	/bin/cp cu /bin/ninstall
	/bin/mv /bin/ninstall /bin/cu </dev/null
clean:
	/bin/rm -f *.b cu
