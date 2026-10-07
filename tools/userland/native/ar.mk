CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: ar
ar: ar.b
	$(CC) -i -s ar.b -o ar
ar.b: /usr/src/cmd/ar.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/ar.c
install: all
	/bin/cp ar /bin/ninstall
	/bin/mv /bin/ninstall /bin/ar </dev/null
clean:
	/bin/rm -f *.b ar
