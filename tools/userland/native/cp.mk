CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: cp
cp: cp.b
	$(CC) -i -s cp.b -o cp
cp.b: /usr/src/cmd/cp.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/cp.c
install: all
	/bin/cp cp /bin/ninstall
	/bin/mv /bin/ninstall /bin/cp </dev/null
clean:
	/bin/rm -f *.b cp
