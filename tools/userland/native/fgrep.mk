CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: fgrep
fgrep: fgrep.b
	$(CC) -i -s fgrep.b -o fgrep
fgrep.b: /usr/src/cmd/fgrep.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/fgrep.c
install: all
	/bin/cp fgrep /bin/ninstall
	/bin/mv /bin/ninstall /bin/fgrep </dev/null
clean:
	/bin/rm -f *.b fgrep
