CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: makekey
makekey: makekey.b
	$(CC) -i -s makekey.b -o makekey
makekey.b: /usr/src/cmd/makekey.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/makekey.c
install: all
	/bin/cp makekey /usr/lib/ninstall
	/bin/mv /usr/lib/ninstall /usr/lib/makekey </dev/null
clean:
	/bin/rm -f *.b makekey
