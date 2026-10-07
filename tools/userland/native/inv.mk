CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: inv
inv: inv1.b inv2.b inv3.b inv5.b inv6.b deliv2.b
	$(CC) -i -s inv1.b inv2.b inv3.b inv5.b inv6.b deliv2.b -o inv
inv1.b: /usr/src/cmd/refer/inv1.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/inv1.c
inv2.b: /usr/src/cmd/refer/inv2.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/inv2.c
inv3.b: /usr/src/cmd/refer/inv3.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/inv3.c
inv5.b: /usr/src/cmd/refer/inv5.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/inv5.c
inv6.b: /usr/src/cmd/refer/inv6.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/inv6.c
deliv2.b: /usr/src/cmd/refer/deliv2.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/deliv2.c
install: all
	/bin/cp inv /usr/lib/refer/ninstall
	/bin/mv /usr/lib/refer/ninstall /usr/lib/refer/inv </dev/null
clean:
	/bin/rm -f *.b inv
