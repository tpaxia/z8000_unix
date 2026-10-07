CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: time
time: time.b
	$(CC) -i -s time.b -o time
time.b: /usr/src/cmd/time.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/time.c
install: all
	/bin/cp time /bin/ninstall
	/bin/mv /bin/ninstall /bin/time </dev/null
clean:
	/bin/rm -f *.b time
