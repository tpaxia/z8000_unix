CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: dcheck
dcheck: dcheck.b
	$(CC) -i -s dcheck.b -o dcheck
dcheck.b: /usr/src/cmd/dcheck.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/dcheck.c
install: all
	/bin/cp dcheck /bin/ninstall
	/bin/mv /bin/ninstall /bin/dcheck </dev/null
clean:
	/bin/rm -f *.b dcheck
