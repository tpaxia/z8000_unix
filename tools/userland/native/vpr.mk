CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: vpr
vpr: vpr.b
	$(CC) -i -s vpr.b -o vpr
vpr.b: /usr/src/cmd/vpr.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/vpr.c
install: all
	/bin/cp vpr /bin/ninstall
	/bin/mv /bin/ninstall /bin/vpr </dev/null
clean:
	/bin/rm -f *.b vpr
