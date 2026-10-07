CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: yacc
yacc: y1.b y2.b y3.b y4.b
	$(CC) -i -s y1.b y2.b y3.b y4.b -o yacc
y1.b: /usr/src/cmd/yacc/y1.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/yacc/y1.c
y2.b: /usr/src/cmd/yacc/y2.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/yacc/y2.c
y3.b: /usr/src/cmd/yacc/y3.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/yacc/y3.c
y4.b: /usr/src/cmd/yacc/y4.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/yacc/y4.c
install: all
	/bin/cp yacc /bin/ninstall
	/bin/mv /bin/ninstall /bin/yacc </dev/null
clean:
	/bin/rm -f *.b yacc
