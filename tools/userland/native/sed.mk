CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: sed
sed: sed0.b sed1.b
	$(CC) -i -s sed0.b sed1.b -o sed
sed0.b: /usr/src/cmd/sed/sed0.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/sed/sed0.c
sed1.b: /usr/src/cmd/sed/sed1.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/sed/sed1.c
install: all
	/bin/cp sed /bin/ninstall
	/bin/mv /bin/ninstall /bin/sed </dev/null
clean:
	/bin/rm -f *.b sed
