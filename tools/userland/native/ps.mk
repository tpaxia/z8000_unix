CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: ps
ps: ps.b
	$(CC) -i -s ps.b -o ps
ps.b: /usr/src/cmd/ps.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/ps.c
install: all
clean:
	/bin/rm -f *.b ps
