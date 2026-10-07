CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: tab832
tab832: tab832.b
	/bin/ldz8 -i -x tab832.b -o tab832
tab832.b: /usr/src/cmd/troff/term/tab832.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/troff/term/tab832.c
install: all
	/bin/cp tab832 /usr/lib/term/ninstall
	/bin/mv /usr/lib/term/ninstall /usr/lib/term/tab832 </dev/null
clean:
	/bin/rm -f *.b tab832
