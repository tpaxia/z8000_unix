CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: sa
sa: sa.b
	$(CC) -i -s sa.b -o sa
sa.b: /usr/src/cmd/sa.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/sa.c
install: all
	/bin/cp sa /bin/ninstall
	/bin/mv /bin/ninstall /bin/sa </dev/null
clean:
	/bin/rm -f *.b sa
