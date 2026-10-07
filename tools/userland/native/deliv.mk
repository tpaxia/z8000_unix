CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: deliv
deliv: deliv1.b deliv2.b
	$(CC) -i -s deliv1.b deliv2.b -o deliv
deliv1.b: /usr/src/cmd/refer/deliv1.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/deliv1.c
deliv2.b: /usr/src/cmd/refer/deliv2.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/deliv2.c
install: all
	/bin/cp deliv /usr/lib/refer/ninstall
	/bin/mv /usr/lib/refer/ninstall /usr/lib/refer/deliv </dev/null
clean:
	/bin/rm -f *.b deliv
