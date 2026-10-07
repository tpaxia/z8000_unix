CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: sync
sync: sync.b
	$(CC) -i -s sync.b -o sync
sync.b: /usr/src/cmd/sync.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/sync.c
install: all
	/bin/cp sync /bin/ninstall
	/bin/mv /bin/ninstall /bin/sync </dev/null
clean:
	/bin/rm -f *.b sync
