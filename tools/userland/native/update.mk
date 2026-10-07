CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: update
update: update.b
	$(CC) -i -s update.b -o update
update.b: /usr/src/cmd/update.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/update.c
install: all
	/bin/cp update /bin/ninstall
	/bin/mv /bin/ninstall /bin/update </dev/null
clean:
	/bin/rm -f *.b update
