CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: pstat
pstat: pstat.b
	$(CC) -i -s pstat.b -o pstat
pstat.b: /usr/src/cmd/pstat.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/pstat.c
install: all
clean:
	/bin/rm -f *.b pstat
