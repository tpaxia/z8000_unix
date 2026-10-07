CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: spell
spell: spell.b
	$(CC) -i -s spell.b -o spell
spell.b: /usr/src/cmd/spell/spell.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/spell/spell.c
install: all
	/bin/cp spell /usr/lib/ninstall
	/bin/mv /usr/lib/ninstall /usr/lib/spell </dev/null
clean:
	/bin/rm -f *.b spell
