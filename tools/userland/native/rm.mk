CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: rm
rm: rm.b
	$(CC) -i -s rm.b -o rm
rm.b: /usr/src/cmd/rm.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/rm.c
install: all
	/bin/cp rm /bin/ninstall
	/bin/mv /bin/ninstall /bin/rm </dev/null
clean:
	/bin/rm -f *.b rm
