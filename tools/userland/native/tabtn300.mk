CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: tabtn300
tabtn300: tabtn300.b
	/bin/ldz8 -i -x tabtn300.b -o tabtn300
tabtn300.b: /usr/src/cmd/troff/term/tabtn300.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/troff/term/tabtn300.c
install: all
	/bin/cp tabtn300 /usr/lib/term/ninstall
	/bin/mv /usr/lib/term/ninstall /usr/lib/term/tabtn300 </dev/null
clean:
	/bin/rm -f *.b tabtn300
