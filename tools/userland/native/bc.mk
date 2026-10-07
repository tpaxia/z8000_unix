CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I. -I/usr/src/cmd
all: bc
bc: y.tab.b
	$(CC) -i -s y.tab.b -o bc
y.tab.c: /usr/src/cmd/bc.y /bin/yacc
	/bin/yacc -d /usr/src/cmd/bc.y
y.tab.b: y.tab.c
	$(CC) $(CFLAGS) -c y.tab.c
install: all
	/bin/cp bc /bin/ninstall
	/bin/mv /bin/ninstall /bin/bc </dev/null
clean:
	/bin/rm -f *.b bc y.tab.c
