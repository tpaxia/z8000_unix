CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: ln
ln: ln.b
	$(CC) -i -s ln.b -o ln
ln.b: /usr/src/cmd/ln.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/ln.c
install: all
	/bin/cp ln /bin/ninstall
	/bin/mv /bin/ninstall /bin/ln </dev/null
clean:
	/bin/rm -f *.b ln
