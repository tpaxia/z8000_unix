CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: size
size: size.b
	$(CC) -i -s size.b -o size
size.b: /usr/src/cmd/size.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/size.c
install: all
	/bin/cp size /bin/ninstall
	/bin/mv /bin/ninstall /bin/size </dev/null
clean:
	/bin/rm -f *.b size
