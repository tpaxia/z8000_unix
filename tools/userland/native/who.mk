CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: who
who: who.b
	$(CC) -i -s who.b -o who
who.b: /usr/src/cmd/who.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/who.c
install: all
	/bin/cp who /bin/ninstall
	/bin/mv /bin/ninstall /bin/who </dev/null
clean:
	/bin/rm -f *.b who
