CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I. -I/usr/src/cmd/struct -I/usr/src/cmd/struct
all: beautify
beautify: tree.b lextab.b bdef.b y.tab.b ../libln/libln.a
	$(CC) -i -s tree.b lextab.b bdef.b y.tab.b ../libln/libln.a -o beautify
y.tab.c: /usr/src/cmd/struct/beauty.y /bin/yacc
	/bin/yacc -d /usr/src/cmd/struct/beauty.y
lextab.c: /usr/src/cmd/struct/lextab.l ../lex/lex
	../lex/lex /usr/src/cmd/struct/lextab.l
	/bin/mv lex.yy.c lextab.c </dev/null
tree.b: /usr/src/cmd/struct/tree.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/tree.c
lextab.b: lextab.c y.tab.c
	$(CC) $(CFLAGS) -c lextab.c
bdef.b: /usr/src/cmd/struct/bdef.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/bdef.c
y.tab.b: y.tab.c
	$(CC) $(CFLAGS) -c y.tab.c
install: all
	/bin/cp beautify /usr/lib/struct/ninstall
	/bin/mv /usr/lib/struct/ninstall /usr/lib/struct/beautify </dev/null
clean:
	/bin/rm -f *.b beautify y.tab.c lextab.c
