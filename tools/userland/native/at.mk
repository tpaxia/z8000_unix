CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: at
at: at.b
	$(CC) -i -s at.b -o at
at.b: /usr/src/cmd/at.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/at.c
install: all
	/bin/cp at /bin/ninstall
	/bin/mv /bin/ninstall /bin/at </dev/null
clean:
	/bin/rm -f *.b at
