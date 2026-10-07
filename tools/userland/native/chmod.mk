CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: chmod
chmod: chmod.b
	$(CC) -i -s chmod.b -o chmod
chmod.b: /usr/src/cmd/chmod.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/chmod.c
install: all
	/bin/cp chmod /bin/ninstall
	/bin/mv /bin/ninstall /bin/chmod </dev/null
clean:
	/bin/rm -f *.b chmod
