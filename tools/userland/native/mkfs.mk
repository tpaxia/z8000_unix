CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: mkfs
mkfs: mkfs.b
	$(CC) -i -s mkfs.b -o mkfs
mkfs.b: /usr/src/cmd/mkfs.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/mkfs.c
install: all
	/bin/cp mkfs /bin/ninstall
	/bin/mv /bin/ninstall /bin/mkfs </dev/null
clean:
	/bin/rm -f *.b mkfs
