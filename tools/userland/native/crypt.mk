CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: crypt
crypt: crypt.b
	$(CC) -i -s crypt.b -o crypt
crypt.b: /usr/src/cmd/crypt.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/crypt.c
install: all
	/bin/cp crypt /bin/ninstall
	/bin/mv /bin/ninstall /bin/crypt </dev/null
clean:
	/bin/rm -f *.b crypt
