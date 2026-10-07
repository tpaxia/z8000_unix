CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: wump
wump: wump.b
	$(CC) -i -s wump.b -o wump
wump.b: /usr/src/games/wump.c
	$(CC) $(CFLAGS) -c /usr/src/games/wump.c
install: all
	/bin/cp wump /usr/games/ninstall
	/bin/mv /usr/games/ninstall /usr/games/wump </dev/null
clean:
	/bin/rm -f *.b wump
