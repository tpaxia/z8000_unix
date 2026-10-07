CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: tabs
tabs: tabs.b
	$(CC) -i -s tabs.b -o tabs
tabs.b: /usr/src/cmd/tabs.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tabs.c
install: all
	/bin/cp tabs /bin/ninstall
	/bin/mv /bin/ninstall /bin/tabs </dev/null
clean:
	/bin/rm -f *.b tabs
