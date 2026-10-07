CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: deroff
deroff: deroff.b
	$(CC) -i -s deroff.b -o deroff
deroff.b: /usr/src/cmd/deroff.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/deroff.c
install: all
	/bin/cp deroff /bin/ninstall
	/bin/mv /bin/ninstall /bin/deroff </dev/null
clean:
	/bin/rm -f *.b deroff
