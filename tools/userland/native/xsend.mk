CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: xsend
xsend: xsend.b lib.b ../libmp/libmp.a
	$(CC) -i -s xsend.b lib.b ../libmp/libmp.a -o xsend
xsend.b: /usr/src/cmd/xsend/xsend.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/xsend/xsend.c
lib.b: /usr/src/cmd/xsend/lib.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/xsend/lib.c
install: all
	/bin/cp xsend /bin/ninstall
	/bin/mv /bin/ninstall /bin/xsend </dev/null
clean:
	/bin/rm -f *.b xsend
