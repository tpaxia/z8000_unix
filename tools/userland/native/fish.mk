CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: fish
fish: fish.b
	$(CC) -i -s fish.b -o fish
fish.b: /usr/src/games/fish.c
	$(CC) $(CFLAGS) -c /usr/src/games/fish.c
install: all
	/bin/cp fish /usr/games/ninstall
	/bin/mv /usr/games/ninstall /usr/games/fish </dev/null
clean:
	/bin/rm -f *.b fish
