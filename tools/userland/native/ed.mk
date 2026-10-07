CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: ed
ed: ed.b
	$(CC) -i -s ed.b -o ed
ed.b: /usr/src/cmd/ed.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/ed.c
install: all
	/bin/cp ed /bin/ninstall
	/bin/mv /bin/ninstall /bin/ed </dev/null
clean:
	/bin/rm -f *.b ed
