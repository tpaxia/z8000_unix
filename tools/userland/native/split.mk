CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: split
split: split.b
	$(CC) -i -s split.b -o split
split.b: /usr/src/cmd/split.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/split.c
install: all
	/bin/cp split /bin/ninstall
	/bin/mv /bin/ninstall /bin/split </dev/null
clean:
	/bin/rm -f *.b split
