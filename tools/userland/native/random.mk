CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: random
random: random.b
	$(CC) -i -s random.b -o random
random.b: /usr/src/cmd/random.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/random.c
install: all
	/bin/cp random /bin/ninstall
	/bin/mv /bin/ninstall /bin/random </dev/null
clean:
	/bin/rm -f *.b random
