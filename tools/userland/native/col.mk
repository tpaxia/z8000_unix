CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: col
col: col.b
	$(CC) -i -s col.b -o col
col.b: /usr/src/cmd/col.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/col.c
install: all
	/bin/cp col /bin/ninstall
	/bin/mv /bin/ninstall /bin/col </dev/null
clean:
	/bin/rm -f *.b col
