CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: chown
chown: chown.b
	$(CC) -i -s chown.b -o chown
chown.b: /usr/src/cmd/chown.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/chown.c
install: all
	/bin/cp chown /bin/ninstall
	/bin/mv /bin/ninstall /bin/chown </dev/null
clean:
	/bin/rm -f *.b chown
