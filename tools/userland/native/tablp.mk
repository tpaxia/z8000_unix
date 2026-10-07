CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: tablp
tablp: tablp.b
	/bin/ldz8 -i -x tablp.b -o tablp
tablp.b: /usr/src/cmd/troff/term/tablp.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/troff/term/tablp.c
install: all
	/bin/cp tablp /usr/lib/term/ninstall
	/bin/mv /usr/lib/term/ninstall /usr/lib/term/tablp </dev/null
clean:
	/bin/rm -f *.b tablp
