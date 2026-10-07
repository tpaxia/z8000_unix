CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: look
look: look.b
	$(CC) -i -s look.b -o look
look.b: /usr/src/cmd/look.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/look.c
install: all
	/bin/cp look /bin/ninstall
	/bin/mv /bin/ninstall /bin/look </dev/null
clean:
	/bin/rm -f *.b look
