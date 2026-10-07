CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: tsort
tsort: tsort.b
	$(CC) -i -s tsort.b -o tsort
tsort.b: /usr/src/cmd/tsort.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tsort.c
install: all
	/bin/cp tsort /bin/ninstall
	/bin/mv /bin/ninstall /bin/tsort </dev/null
clean:
	/bin/rm -f *.b tsort
