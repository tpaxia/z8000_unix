CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: mkdir
mkdir: mkdir.b
	$(CC) -i -s mkdir.b -o mkdir
mkdir.b: /usr/src/cmd/mkdir.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/mkdir.c
install: all
	/bin/cp mkdir /bin/ninstall
	/bin/mv /bin/ninstall /bin/mkdir </dev/null
	/bin/chmod 4755 /bin/mkdir
clean:
	/bin/rm -f *.b mkdir
