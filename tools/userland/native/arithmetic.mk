CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: arithmetic
arithmetic: arithmetic.b
	$(CC) -i -s arithmetic.b -o arithmetic
arithmetic.b: /usr/src/games/arithmetic.c
	$(CC) $(CFLAGS) -c /usr/src/games/arithmetic.c
install: all
	/bin/cp arithmetic /usr/games/ninstall
	/bin/mv /usr/games/ninstall /usr/games/arithmetic </dev/null
clean:
	/bin/rm -f *.b arithmetic
