CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: tc
tc: tc.b
	$(CC) -i -s tc.b -o tc
tc.b: /usr/src/cmd/tc.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tc.c
install: all
	/bin/cp tc /bin/ninstall
	/bin/mv /bin/ninstall /bin/tc </dev/null
clean:
	/bin/rm -f *.b tc
