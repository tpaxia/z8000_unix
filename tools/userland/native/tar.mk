CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: tar
tar: tar.b
	$(CC) -i -s tar.b -o tar
tar.b: /usr/src/cmd/tar/tar.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tar/tar.c
install: all
	/bin/cp tar /bin/ninstall
	/bin/mv /bin/ninstall /bin/tar </dev/null
clean:
	/bin/rm -f *.b tar
