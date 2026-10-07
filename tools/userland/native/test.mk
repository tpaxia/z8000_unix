CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: test
test: test.b
	$(CC) -i -s test.b -o test
test.b: /usr/src/cmd/test.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/test.c
install: all
	/bin/cp test /bin/ninstall
	/bin/mv /bin/ninstall /bin/test </dev/null
clean:
	/bin/rm -f *.b test
