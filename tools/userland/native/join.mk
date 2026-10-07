CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: join
join: join.b
	$(CC) -i -s join.b -o join
join.b: /usr/src/cmd/join.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/join.c
install: all
	/bin/cp join /bin/ninstall
	/bin/mv /bin/ninstall /bin/join </dev/null
clean:
	/bin/rm -f *.b join
