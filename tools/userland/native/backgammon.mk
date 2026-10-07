CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: backgammon
backgammon: backgammon.b
	$(CC) -i -s backgammon.b -o backgammon
backgammon.b: /usr/src/games/backgammon.c
	$(CC) $(CFLAGS) -c /usr/src/games/backgammon.c
install: all
	/bin/cp backgammon /usr/games/ninstall
	/bin/mv /usr/games/ninstall /usr/games/backgammon </dev/null
clean:
	/bin/rm -f *.b backgammon
