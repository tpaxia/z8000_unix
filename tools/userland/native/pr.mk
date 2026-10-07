CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: pr
pr: pr.b
	$(CC) -i -s pr.b -o pr
pr.b: /usr/src/cmd/pr.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/pr.c
install: all
	/bin/cp pr /bin/ninstall
	/bin/mv /bin/ninstall /bin/pr </dev/null
clean:
	/bin/rm -f *.b pr
