CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: find
find: find.b
	$(CC) -i -s find.b -o find
find.b: /usr/src/cmd/find.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/find.c
install: all
	/bin/cp find /bin/ninstall
	/bin/mv /bin/ninstall /bin/find </dev/null
clean:
	/bin/rm -f *.b find
