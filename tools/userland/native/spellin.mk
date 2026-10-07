CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: spellin
spellin: spellin.b
	$(CC) -i -s spellin.b -o spellin
spellin.b: /usr/src/cmd/spell/spellin.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/spell/spellin.c
install: all
	/bin/cp spellin /usr/lib/ninstall
	/bin/mv /usr/lib/ninstall /usr/lib/spellin </dev/null
clean:
	/bin/rm -f *.b spellin
