CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: tp
tp: tp0.b tp1.b tp2.b tp3.b
	$(CC) -i -s tp0.b tp1.b tp2.b tp3.b -o tp
tp0.b: /usr/src/cmd/tp/tp0.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tp/tp0.c
tp1.b: /usr/src/cmd/tp/tp1.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tp/tp1.c
tp2.b: /usr/src/cmd/tp/tp2.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tp/tp2.c
tp3.b: /usr/src/cmd/tp/tp3.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tp/tp3.c
install: all
	/bin/cp tp /bin/ninstall
	/bin/mv /bin/ninstall /bin/tp </dev/null
clean:
	/bin/rm -f *.b tp
