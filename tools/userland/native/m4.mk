CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I. -I/usr/src/cmd/m4
all: m4
m4: m4.b y.tab.b
	$(CC) -i -s m4.b y.tab.b -o m4
y.tab.c: /usr/src/cmd/m4/m4y.y /bin/yacc
	/bin/yacc -d /usr/src/cmd/m4/m4y.y
m4.b: /usr/src/cmd/m4/m4.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/m4/m4.c
y.tab.b: y.tab.c
	$(CC) $(CFLAGS) -c y.tab.c
install: all
	/bin/cp m4 /bin/ninstall
	/bin/mv /bin/ninstall /bin/m4 </dev/null
clean:
	/bin/rm -f *.b m4 y.tab.c
