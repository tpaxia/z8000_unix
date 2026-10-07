CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: tab300s-12
tab300s-12: tab300s-12.b
	/bin/ldz8 -i -x tab300s-12.b -o tab300s-12
tab300s-12.b: /usr/src/cmd/troff/term/tab300s-12.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/troff/term/tab300s-12.c
install: all
	/bin/cp tab300s-12 /usr/lib/term/ninstall
	/bin/mv /usr/lib/term/ninstall /usr/lib/term/tab300s-12 </dev/null
clean:
	/bin/rm -f *.b tab300s-12
