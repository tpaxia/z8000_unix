CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: umount
umount: umount.b
	$(CC) -i -s umount.b -o umount
umount.b: /usr/src/cmd/umount.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/umount.c
install: all
	/bin/cp umount /bin/ninstall
	/bin/mv /bin/ninstall /bin/umount </dev/null
clean:
	/bin/rm -f *.b umount
