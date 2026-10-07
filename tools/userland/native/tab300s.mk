CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: tab300s
tab300s: tab300s.b
	/bin/ldz8 -i -x tab300s.b -o tab300s
tab300s.b: /usr/src/cmd/troff/term/tab300s.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/troff/term/tab300s.c
install: all
	/bin/cp tab300s /usr/lib/term/ninstall
	/bin/mv /usr/lib/term/ninstall /usr/lib/term/tab300s </dev/null
clean:
	/bin/rm -f *.b tab300s
