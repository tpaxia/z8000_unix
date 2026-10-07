CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I. -DNEQN -I/usr/src/cmd/neqn
all: neqn
neqn: diacrit.b eqnbox.b font.b fromto.b funny.b glob.b integral.b io.b lex.b lookup.b mark.b matrix.b move.b over.b paren.b pile.b shift.b size.b sqrt.b text.b y.tab.b
	$(CC) -i -s diacrit.b eqnbox.b font.b fromto.b funny.b glob.b integral.b io.b lex.b lookup.b mark.b matrix.b move.b over.b paren.b pile.b shift.b size.b sqrt.b text.b y.tab.b -o neqn
y.tab.c: /usr/src/cmd/neqn/e.y /bin/yacc
	/bin/yacc -d /usr/src/cmd/neqn/e.y
	/bin/cp y.tab.h e.def
diacrit.b: /usr/src/cmd/neqn/diacrit.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/neqn/diacrit.c
eqnbox.b: /usr/src/cmd/neqn/eqnbox.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/neqn/eqnbox.c
font.b: /usr/src/cmd/neqn/font.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/neqn/font.c
fromto.b: /usr/src/cmd/neqn/fromto.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/neqn/fromto.c
funny.b: /usr/src/cmd/neqn/funny.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/neqn/funny.c
glob.b: /usr/src/cmd/neqn/glob.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/neqn/glob.c
integral.b: /usr/src/cmd/neqn/integral.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/neqn/integral.c
io.b: /usr/src/cmd/neqn/io.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/neqn/io.c
lex.b: /usr/src/cmd/neqn/lex.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/neqn/lex.c
lookup.b: /usr/src/cmd/neqn/lookup.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/neqn/lookup.c
mark.b: /usr/src/cmd/neqn/mark.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/neqn/mark.c
matrix.b: /usr/src/cmd/neqn/matrix.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/neqn/matrix.c
move.b: /usr/src/cmd/neqn/move.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/neqn/move.c
over.b: /usr/src/cmd/neqn/over.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/neqn/over.c
paren.b: /usr/src/cmd/neqn/paren.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/neqn/paren.c
pile.b: /usr/src/cmd/neqn/pile.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/neqn/pile.c
shift.b: /usr/src/cmd/neqn/shift.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/neqn/shift.c
size.b: /usr/src/cmd/neqn/size.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/neqn/size.c
sqrt.b: /usr/src/cmd/neqn/sqrt.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/neqn/sqrt.c
text.b: /usr/src/cmd/neqn/text.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/neqn/text.c
y.tab.b: y.tab.c
	$(CC) $(CFLAGS) -c y.tab.c
install: all
	/bin/cp neqn /bin/ninstall
	/bin/mv /bin/ninstall /bin/neqn </dev/null
clean:
	/bin/rm -f *.b neqn y.tab.c
