CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: comm
comm: comm.b
	$(CC) -i -s comm.b -o comm
comm.b: /usr/src/cmd/comm.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/comm.c
install: all
	/bin/cp comm /bin/ninstall
	/bin/mv /bin/ninstall /bin/comm </dev/null
clean:
	/bin/rm -f *.b comm
