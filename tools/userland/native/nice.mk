CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: nice
nice: nice.b
	$(CC) -i -s nice.b -o nice
nice.b: /usr/src/cmd/nice.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/nice.c
install: all
	/bin/cp nice /bin/ninstall
	/bin/mv /bin/ninstall /bin/nice </dev/null
clean:
	/bin/rm -f *.b nice
