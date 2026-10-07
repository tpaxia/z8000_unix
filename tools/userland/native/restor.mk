CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: restor
restor: restor.b
	$(CC) -i -s restor.b -o restor
restor.b: /usr/src/cmd/restor.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/restor.c
install: all
	/bin/cp restor /bin/ninstall
	/bin/mv /bin/ninstall /bin/restor </dev/null
clean:
	/bin/rm -f *.b restor
