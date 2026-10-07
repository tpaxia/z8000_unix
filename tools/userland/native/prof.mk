CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: prof
prof: prof.b
	$(CC) -i -s prof.b -o prof
prof.b: /usr/src/cmd/prof.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/prof.c
install: all
	/bin/cp prof /bin/ninstall
	/bin/mv /bin/ninstall /bin/prof </dev/null
clean:
	/bin/rm -f *.b prof
