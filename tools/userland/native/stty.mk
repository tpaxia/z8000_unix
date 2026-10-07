CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: stty
stty: stty.b
	$(CC) -i -s stty.b -o stty
stty.b: /usr/src/cmd/stty.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/stty.c
install: all
	/bin/cp stty /bin/ninstall
	/bin/mv /bin/ninstall /bin/stty </dev/null
clean:
	/bin/rm -f *.b stty
