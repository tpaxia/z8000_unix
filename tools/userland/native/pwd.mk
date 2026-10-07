CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: pwd
pwd: pwd.b
	$(CC) -i -s pwd.b -o pwd
pwd.b: /usr/src/cmd/pwd.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/pwd.c
install: all
	/bin/cp pwd /bin/ninstall
	/bin/mv /bin/ninstall /bin/pwd </dev/null
clean:
	/bin/rm -f *.b pwd
