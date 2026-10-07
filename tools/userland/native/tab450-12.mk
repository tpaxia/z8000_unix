CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: tab450-12
tab450-12: tab450-12.b
	/bin/ldz8 -i -x tab450-12.b -o tab450-12
tab450-12.b: /usr/src/cmd/troff/term/tab450-12.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/troff/term/tab450-12.c
install: all
	/bin/cp tab450-12 /usr/lib/term/ninstall
	/bin/mv /usr/lib/term/ninstall /usr/lib/term/tab450-12 </dev/null
clean:
	/bin/rm -f *.b tab450-12
