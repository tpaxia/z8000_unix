CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: units
units: units.b
	$(CC) -i -s units.b -o units
units.b: /usr/src/cmd/units.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/units.c
install: all
	/bin/cp units /bin/ninstall
	/bin/mv /bin/ninstall /bin/units </dev/null
clean:
	/bin/rm -f *.b units
