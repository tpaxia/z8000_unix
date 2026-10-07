CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I. -I/usr/src/cmd/lex
all: lex
lex: lmain.b sub1.b sub2.b header.b y.tab.b
	$(CC) -i -s lmain.b sub1.b sub2.b header.b y.tab.b -o lex
y.tab.c: /usr/src/cmd/lex/parser.y /bin/yacc
	/bin/yacc -d /usr/src/cmd/lex/parser.y
lmain.b: /usr/src/cmd/lex/lmain.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/lex/lmain.c
sub1.b: /usr/src/cmd/lex/sub1.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/lex/sub1.c
sub2.b: /usr/src/cmd/lex/sub2.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/lex/sub2.c
header.b: /usr/src/cmd/lex/header.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/lex/header.c
y.tab.b: y.tab.c
	$(CC) $(CFLAGS) -c y.tab.c
install: all
	/bin/cp lex /bin/ninstall
	/bin/mv /bin/ninstall /bin/lex </dev/null
clean:
	/bin/rm -f *.b lex y.tab.c
