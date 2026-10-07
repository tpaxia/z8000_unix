CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: cat
cat: cat.b
	$(CC) -i -s cat.b -o cat
cat.b: /usr/src/cmd/cat.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/cat.c
install: all
	/bin/cp cat /bin/ninstall
	/bin/mv /bin/ninstall /bin/cat </dev/null
clean:
	/bin/rm -f *.b cat
