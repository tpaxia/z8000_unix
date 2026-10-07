CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: fortune
fortune: fortune.b
	$(CC) -i -s fortune.b -o fortune
fortune.b: /usr/src/games/fortune.c
	$(CC) $(CFLAGS) -c /usr/src/games/fortune.c
install: all
	/bin/cp fortune /usr/games/ninstall
	/bin/mv /usr/games/ninstall /usr/games/fortune </dev/null
clean:
	/bin/rm -f *.b fortune
