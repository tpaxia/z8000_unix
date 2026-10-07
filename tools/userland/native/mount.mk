CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: mount
mount: mount.b
	$(CC) -i -s mount.b -o mount
mount.b: /usr/src/cmd/mount.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/mount.c
install: all
	/bin/cp mount /bin/ninstall
	/bin/mv /bin/ninstall /bin/mount </dev/null
clean:
	/bin/rm -f *.b mount
