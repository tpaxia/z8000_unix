CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: quot
quot: quot.b
	$(CC) -i -s quot.b -o quot
quot.b: /usr/src/cmd/quot.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/quot.c
install: all
	/bin/cp quot /bin/ninstall
	/bin/mv /bin/ninstall /bin/quot </dev/null
clean:
	/bin/rm -f *.b quot
