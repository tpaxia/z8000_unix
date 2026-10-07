CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: tab37
tab37: tab37.b
	/bin/ldz8 -i -x tab37.b -o tab37
tab37.b: /usr/src/cmd/troff/term/tab37.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/troff/term/tab37.c
install: all
	/bin/cp tab37 /usr/lib/term/ninstall
	/bin/mv /usr/lib/term/ninstall /usr/lib/term/tab37 </dev/null
clean:
	/bin/rm -f *.b tab37
