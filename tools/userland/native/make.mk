CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I. -I/usr/src/cmd/make
all: make
make: ident.b main.b doname.b misc.b files.b dosys.b y.tab.b
	$(CC) -i -s ident.b main.b doname.b misc.b files.b dosys.b y.tab.b -o make
y.tab.c: /usr/src/cmd/make/gram.y /bin/yacc
	/bin/yacc -d /usr/src/cmd/make/gram.y
ident.b: /usr/src/cmd/make/ident.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/make/ident.c
main.b: /usr/src/cmd/make/main.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/make/main.c
doname.b: /usr/src/cmd/make/doname.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/make/doname.c
misc.b: /usr/src/cmd/make/misc.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/make/misc.c
files.b: /usr/src/cmd/make/files.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/make/files.c
dosys.b: /usr/src/cmd/make/dosys.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/make/dosys.c
y.tab.b: y.tab.c
	$(CC) $(CFLAGS) -c y.tab.c
install: all
	/bin/cp make /bin/ninstall
	/bin/ln /bin/make /bin/make.$$$$ && /bin/mv /bin/ninstall /bin/make </dev/null
clean:
	/bin/rm -f *.b make y.tab.c
