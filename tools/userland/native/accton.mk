CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: accton
accton: accton.b
	$(CC) -i -s accton.b -o accton
accton.b: /usr/src/cmd/accton.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/accton.c
install: all
	/bin/cp accton /bin/ninstall
	/bin/mv /bin/ninstall /bin/accton </dev/null
clean:
	/bin/rm -f *.b accton
