CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: uniq
uniq: uniq.b
	$(CC) -i -s uniq.b -o uniq
uniq.b: /usr/src/cmd/uniq.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uniq.c
install: all
	/bin/cp uniq /bin/ninstall
	/bin/mv /bin/ninstall /bin/uniq </dev/null
clean:
	/bin/rm -f *.b uniq
