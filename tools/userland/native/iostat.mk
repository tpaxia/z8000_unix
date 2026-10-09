CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: iostat
iostat: iostat.b
	$(CC) -i -s iostat.b -o iostat
iostat.b: /usr/src/cmd/iostat.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/iostat.c
install: all
	/bin/cp iostat /bin/iostat
clean:
	/bin/rm -f *.b iostat
