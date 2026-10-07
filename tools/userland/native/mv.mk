CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: mv
mv: mv.b
	$(CC) -i -s mv.b -o mv
mv.b: /usr/src/cmd/mv.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/mv.c
install: all
	/bin/cp mv /bin/ninstall
	/bin/ln /bin/mv /bin/mv.$$$$ && /bin/mv /bin/ninstall /bin/mv </dev/null
	/bin/chmod 4755 /bin/mv
clean:
	/bin/rm -f *.b mv
