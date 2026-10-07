CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: tty
tty: tty.b
	$(CC) -i -s tty.b -o tty
tty.b: /usr/src/cmd/tty.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tty.c
install: all
	/bin/cp tty /bin/ninstall
	/bin/mv /bin/ninstall /bin/tty </dev/null
clean:
	/bin/rm -f *.b tty
