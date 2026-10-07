CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: strip
strip: strip.b
	$(CC) -i -s strip.b -o strip
strip.b: /usr/src/cmd/strip.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/strip.c
install: all
	/bin/cp strip /bin/ninstall
	/bin/mv /bin/ninstall /bin/strip </dev/null
clean:
	/bin/rm -f *.b strip
