CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: basename
basename: basename.b
	$(CC) -i -s basename.b -o basename
basename.b: /usr/src/cmd/basename.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/basename.c
install: all
	/bin/cp basename /bin/ninstall
	/bin/mv /bin/ninstall /bin/basename </dev/null
clean:
	/bin/rm -f *.b basename
