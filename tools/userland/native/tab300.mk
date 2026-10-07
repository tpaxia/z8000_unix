CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: tab300
tab300: tab300.b
	/bin/ldz8 -i -x tab300.b -o tab300
tab300.b: /usr/src/cmd/troff/term/tab300.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/troff/term/tab300.c
install: all
	/bin/cp tab300 /usr/lib/term/ninstall
	/bin/mv /usr/lib/term/ninstall /usr/lib/term/tab300 </dev/null
clean:
	/bin/rm -f *.b tab300
