CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: rmdir
rmdir: rmdir.b
	$(CC) -i -s rmdir.b -o rmdir
rmdir.b: /usr/src/cmd/rmdir.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/rmdir.c
install: all
	/bin/cp rmdir /bin/ninstall
	/bin/mv /bin/ninstall /bin/rmdir </dev/null
	/bin/chmod 4755 /bin/rmdir
clean:
	/bin/rm -f *.b rmdir
