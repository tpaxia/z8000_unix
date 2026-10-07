CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: ls
ls: ls.b
	$(CC) -i -s ls.b -o ls
ls.b: /usr/src/cmd/ls.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/ls.c
install: all
	/bin/cp ls /bin/ninstall
	/bin/mv /bin/ninstall /bin/ls </dev/null
clean:
	/bin/rm -f *.b ls
