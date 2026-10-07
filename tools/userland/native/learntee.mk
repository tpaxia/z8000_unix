CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: learntee
learntee: tee.b
	$(CC) -i -s tee.b -o learntee
tee.b: /usr/src/cmd/learn/tee.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/learn/tee.c
install: all
	/bin/cp learntee /bin/ninstall
	/bin/mv /bin/ninstall /bin/learntee </dev/null
clean:
	/bin/rm -f *.b learntee
