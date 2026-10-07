CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I. -I/usr/src/cmd/mip
all: lint2
lint2: lpass2.b
	$(CC) -i -s lpass2.b -o lint2
lpass2.b: /usr/src/cmd/lint/lpass2.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/lint/lpass2.c
install: all
	/bin/cp lint2 /usr/lib/ninstall
	/bin/mv /usr/lib/ninstall /usr/lib/lint2 </dev/null
clean:
	/bin/rm -f *.b lint2
