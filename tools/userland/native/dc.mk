CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: dc
dc: dc.b
	$(CC) -i -s dc.b -o dc
dc.b: /usr/src/cmd/dc/dc.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/dc/dc.c
install: all
	/bin/cp dc /bin/ninstall
	/bin/mv /bin/ninstall /bin/dc </dev/null
clean:
	/bin/rm -f *.b dc
