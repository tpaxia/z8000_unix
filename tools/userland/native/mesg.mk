CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: mesg
mesg: mesg.b
	$(CC) -i -s mesg.b -o mesg
mesg.b: /usr/src/cmd/mesg.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/mesg.c
install: all
	/bin/cp mesg /bin/ninstall
	/bin/mv /bin/ninstall /bin/mesg </dev/null
clean:
	/bin/rm -f *.b mesg
