CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: mknod
mknod: mknod.b
	$(CC) -i -s mknod.b -o mknod
mknod.b: /usr/src/cmd/mknod.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/mknod.c
install: all
	/bin/cp mknod /bin/ninstall
	/bin/mv /bin/ninstall /bin/mknod </dev/null
clean:
	/bin/rm -f *.b mknod
