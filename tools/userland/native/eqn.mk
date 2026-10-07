CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I. -I/usr/src/cmd/eqn
all: eqn
eqn: diacrit.b eqnbox.b font.b fromto.b funny.b glob.b integral.b io.b lex.b lookup.b mark.b matrix.b move.b over.b paren.b pile.b shift.b size.b sqrt.b text.b y.tab.b
	$(CC) -i -s diacrit.b eqnbox.b font.b fromto.b funny.b glob.b integral.b io.b lex.b lookup.b mark.b matrix.b move.b over.b paren.b pile.b shift.b size.b sqrt.b text.b y.tab.b -o eqn
y.tab.c: /usr/src/cmd/eqn/e.y /bin/yacc
	/bin/yacc -d /usr/src/cmd/eqn/e.y
	/bin/cp y.tab.h e.def
diacrit.b: /usr/src/cmd/eqn/diacrit.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/eqn/diacrit.c
eqnbox.b: /usr/src/cmd/eqn/eqnbox.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/eqn/eqnbox.c
font.b: /usr/src/cmd/eqn/font.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/eqn/font.c
fromto.b: /usr/src/cmd/eqn/fromto.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/eqn/fromto.c
funny.b: /usr/src/cmd/eqn/funny.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/eqn/funny.c
glob.b: /usr/src/cmd/eqn/glob.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/eqn/glob.c
integral.b: /usr/src/cmd/eqn/integral.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/eqn/integral.c
io.b: /usr/src/cmd/eqn/io.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/eqn/io.c
lex.b: /usr/src/cmd/eqn/lex.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/eqn/lex.c
lookup.b: /usr/src/cmd/eqn/lookup.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/eqn/lookup.c
mark.b: /usr/src/cmd/eqn/mark.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/eqn/mark.c
matrix.b: /usr/src/cmd/eqn/matrix.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/eqn/matrix.c
move.b: /usr/src/cmd/eqn/move.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/eqn/move.c
over.b: /usr/src/cmd/eqn/over.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/eqn/over.c
paren.b: /usr/src/cmd/eqn/paren.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/eqn/paren.c
pile.b: /usr/src/cmd/eqn/pile.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/eqn/pile.c
shift.b: /usr/src/cmd/eqn/shift.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/eqn/shift.c
size.b: /usr/src/cmd/eqn/size.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/eqn/size.c
sqrt.b: /usr/src/cmd/eqn/sqrt.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/eqn/sqrt.c
text.b: /usr/src/cmd/eqn/text.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/eqn/text.c
y.tab.b: y.tab.c
	$(CC) $(CFLAGS) -c y.tab.c
install: all
	/bin/cp eqn /bin/ninstall
	/bin/mv /bin/ninstall /bin/eqn </dev/null
clean:
	/bin/rm -f *.b eqn y.tab.c
