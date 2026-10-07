CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: wall
wall: wall.b
	$(CC) -i -s wall.b -o wall
wall.b: /usr/src/cmd/wall.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/wall.c
install: all
	/bin/cp wall /bin/ninstall
	/bin/mv /bin/ninstall /bin/wall </dev/null
clean:
	/bin/rm -f *.b wall
