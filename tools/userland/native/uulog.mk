CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: uulog
uulog: uulog.b prefix.b xqt.b ulockf.b gnamef.b
	$(CC) -i -s uulog.b prefix.b xqt.b ulockf.b gnamef.b -o uulog
uulog.b: /usr/src/cmd/uucp/uulog.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/uulog.c
prefix.b: /usr/src/cmd/uucp/prefix.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/prefix.c
xqt.b: /usr/src/cmd/uucp/xqt.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/xqt.c
ulockf.b: /usr/src/cmd/uucp/ulockf.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/ulockf.c
gnamef.b: /usr/src/cmd/uucp/gnamef.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/gnamef.c
install: all
	/bin/cp uulog /bin/ninstall
	/bin/mv /bin/ninstall /bin/uulog </dev/null
clean:
	/bin/rm -f *.b uulog
