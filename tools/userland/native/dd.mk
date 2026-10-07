CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: dd
dd: dd.b
	$(CC) -i -s dd.b -o dd
dd.b: /usr/src/cmd/dd.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/dd.c
install: all
	/bin/cp dd /bin/ninstall
	/bin/mv /bin/ninstall /bin/dd </dev/null
clean:
	/bin/rm -f *.b dd
