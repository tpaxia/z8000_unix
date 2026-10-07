CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: passwd
passwd: passwd.b
	$(CC) -i -s passwd.b -o passwd
passwd.b: /usr/src/cmd/passwd.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/passwd.c
install: all
	/bin/cp passwd /bin/ninstall
	/bin/mv /bin/ninstall /bin/passwd </dev/null
clean:
	/bin/rm -f *.b passwd
