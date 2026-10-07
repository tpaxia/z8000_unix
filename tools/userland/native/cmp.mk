CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: cmp
cmp: cmp.b
	$(CC) -i -s cmp.b -o cmp
cmp.b: /usr/src/cmd/cmp.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/cmp.c
install: all
	/bin/cp cmp /bin/ninstall
	/bin/mv /bin/ninstall /bin/cmp </dev/null
clean:
	/bin/rm -f *.b cmp
