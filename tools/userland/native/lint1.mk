CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I. -I/usr/src/cmd/lint -I/usr/src/cmd/mip -I/usr/src/cmd/mip
all: lint1
lint1: xdefs.b scan.b comm1.b pftn.b trees.b optim.b lint.b y.tab.b
	$(CC) -i -s xdefs.b scan.b comm1.b pftn.b trees.b optim.b lint.b y.tab.b -o lint1
y.tab.c: /usr/src/cmd/mip/cgram.y /bin/yacc
	/bin/yacc -d /usr/src/cmd/mip/cgram.y
xdefs.b: /usr/src/cmd/mip/xdefs.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/mip/xdefs.c
scan.b: /usr/src/cmd/mip/scan.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/mip/scan.c
comm1.b: /usr/src/cmd/mip/comm1.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/mip/comm1.c
pftn.b: /usr/src/cmd/mip/pftn.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/mip/pftn.c
trees.b: /usr/src/cmd/mip/trees.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/mip/trees.c
optim.b: /usr/src/cmd/mip/optim.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/mip/optim.c
lint.b: /usr/src/cmd/lint/lint.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/lint/lint.c
y.tab.b: y.tab.c
	$(CC) $(CFLAGS) -c y.tab.c
install: all
	/bin/cp lint1 /usr/lib/ninstall
	/bin/mv /usr/lib/ninstall /usr/lib/lint1 </dev/null
clean:
	/bin/rm -f *.b lint1 y.tab.c
