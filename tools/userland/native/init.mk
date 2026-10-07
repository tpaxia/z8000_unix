CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: init
init: init.b
	$(CC) -i -s init.b -o init
init.b: /usr/src/cmd/init.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/init.c
install: all
	/bin/cp init /etc/ninstall
	/bin/mv /etc/ninstall /etc/init.v7 </dev/null
clean:
	/bin/rm -f *.b init
