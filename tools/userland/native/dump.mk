CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: dump
dump: dump.b
	$(CC) -i -s dump.b -o dump
dump.b: /usr/src/cmd/dump.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/dump.c
install: all
	/bin/cp dump /bin/ninstall
	/bin/mv /bin/ninstall /bin/dump </dev/null
clean:
	/bin/rm -f *.b dump
