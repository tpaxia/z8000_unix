CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: sum
sum: sum.b
	$(CC) -i -s sum.b -o sum
sum.b: /usr/src/cmd/sum.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/sum.c
install: all
	/bin/cp sum /bin/ninstall
	/bin/mv /bin/ninstall /bin/sum </dev/null
clean:
	/bin/rm -f *.b sum
