CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002
all: savecore
savecore: savecore.b
	$(CC) -i -s savecore.b -o savecore
savecore.b: /usr/src/savecore.c
	$(CC) $(CFLAGS) -c /usr/src/savecore.c
install: all
	/bin/cp savecore /bin/savecore
clean:
	/bin/rm -f *.b savecore
