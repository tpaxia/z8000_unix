CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: sort
sort: sort.b
	$(CC) -i -s sort.b -o sort
sort.b: /usr/src/cmd/sort.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/sort.c
install: all
	/bin/cp sort /bin/ninstall
	/bin/mv /bin/ninstall /bin/sort </dev/null
clean:
	/bin/rm -f *.b sort
