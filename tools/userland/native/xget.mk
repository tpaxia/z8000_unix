CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: xget
xget: xget.b lib.b ../libmp/libmp.a
	$(CC) -i -s xget.b lib.b ../libmp/libmp.a -o xget
xget.b: /usr/src/cmd/xsend/xget.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/xsend/xget.c
lib.b: /usr/src/cmd/xsend/lib.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/xsend/lib.c
install: all
	/bin/cp xget /bin/ninstall
	/bin/mv /bin/ninstall /bin/xget </dev/null
clean:
	/bin/rm -f *.b xget
