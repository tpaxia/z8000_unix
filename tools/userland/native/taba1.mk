CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: taba1
taba1: taba1.b
	/bin/ldz8 -i -x taba1.b -o taba1
taba1.b: /usr/src/cmd/troff/term/taba1.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/troff/term/taba1.c
install: all
	/bin/cp taba1 /usr/lib/term/ninstall
	/bin/mv /usr/lib/term/ninstall /usr/lib/term/taba1 </dev/null
clean:
	/bin/rm -f *.b taba1
