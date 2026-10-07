CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: tee
tee: tee.b
	$(CC) -i -s tee.b -o tee
tee.b: /usr/src/cmd/tee.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tee.c
install: all
	/bin/cp tee /bin/ninstall
	/bin/mv /bin/ninstall /bin/tee </dev/null
clean:
	/bin/rm -f *.b tee
