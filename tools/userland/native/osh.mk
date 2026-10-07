CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: osh
osh: osh.b
	$(CC) -i -s osh.b -o osh
osh.b: /usr/src/cmd/osh.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/osh.c
install: all
	/bin/cp osh /bin/ninstall
	/bin/mv /bin/ninstall /bin/osh </dev/null
clean:
	/bin/rm -f *.b osh
