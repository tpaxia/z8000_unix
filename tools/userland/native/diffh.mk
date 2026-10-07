CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: diffh
diffh: diffh.b
	$(CC) -i -s diffh.b -o diffh
diffh.b: /usr/src/cmd/diffh.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/diffh.c
install: all
	/bin/cp diffh /usr/lib/ninstall
	/bin/mv /usr/lib/ninstall /usr/lib/diffh </dev/null
clean:
	/bin/rm -f *.b diffh
