CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: mkey
mkey: mkey1.b mkey2.b mkey3.b deliv2.b
	$(CC) -i -s mkey1.b mkey2.b mkey3.b deliv2.b -o mkey
mkey1.b: /usr/src/cmd/refer/mkey1.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/mkey1.c
mkey2.b: /usr/src/cmd/refer/mkey2.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/mkey2.c
mkey3.b: /usr/src/cmd/refer/mkey3.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/mkey3.c
deliv2.b: /usr/src/cmd/refer/deliv2.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/deliv2.c
install: all
	/bin/cp mkey /usr/lib/refer/ninstall
	/bin/mv /usr/lib/refer/ninstall /usr/lib/refer/mkey </dev/null
clean:
	/bin/rm -f *.b mkey
