CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: enroll
enroll: enroll.b lib.b ../libmp/libmp.a
	$(CC) -i -s enroll.b lib.b ../libmp/libmp.a -o enroll
enroll.b: /usr/src/cmd/xsend/enroll.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/xsend/enroll.c
lib.b: /usr/src/cmd/xsend/lib.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/xsend/lib.c
install: all
	/bin/cp enroll /bin/ninstall
	/bin/mv /bin/ninstall /bin/enroll </dev/null
clean:
	/bin/rm -f *.b enroll
