CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: ac
ac: ac.b
	$(CC) -i -s ac.b -o ac
ac.b: /usr/src/cmd/ac.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/ac.c
install: all
	/bin/cp ac /bin/ninstall
	/bin/mv /bin/ninstall /bin/ac </dev/null
clean:
	/bin/rm -f *.b ac
