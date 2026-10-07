CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: lcount
lcount: lcount.b
	$(CC) -i -s lcount.b -o lcount
lcount.b: /usr/src/cmd/learn/lcount.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/learn/lcount.c
install: all
	/bin/cp lcount /bin/ninstall
	/bin/mv /bin/ninstall /bin/lcount </dev/null
clean:
	/bin/rm -f *.b lcount
