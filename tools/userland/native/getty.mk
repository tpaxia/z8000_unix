CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: getty
getty: getty.b
	$(CC) -i -s getty.b -o getty
getty.b: /usr/src/cmd/getty.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/getty.c
install: all
	/bin/cp getty /bin/ninstall
	/bin/mv /bin/ninstall /bin/getty </dev/null
clean:
	/bin/rm -f *.b getty
