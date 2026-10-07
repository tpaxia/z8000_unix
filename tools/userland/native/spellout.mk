CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: spellout
spellout: spellout.b
	$(CC) -i -s spellout.b -o spellout
spellout.b: /usr/src/cmd/spell/spellout.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/spell/spellout.c
install: all
	/bin/cp spellout /usr/lib/ninstall
	/bin/mv /usr/lib/ninstall /usr/lib/spellout </dev/null
clean:
	/bin/rm -f *.b spellout
