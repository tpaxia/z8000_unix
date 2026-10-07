CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: kill
kill: kill.b
	$(CC) -i -s kill.b -o kill
kill.b: /usr/src/cmd/kill.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/kill.c
install: all
	/bin/cp kill /bin/ninstall
	/bin/mv /bin/ninstall /bin/kill </dev/null
clean:
	/bin/rm -f *.b kill
