CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: wc
wc: wc.b
	$(CC) -i -s wc.b -o wc
wc.b: /usr/src/cmd/wc.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/wc.c
install: all
	/bin/cp wc /bin/ninstall
	/bin/mv /bin/ninstall /bin/wc </dev/null
clean:
	/bin/rm -f *.b wc
