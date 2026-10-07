CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I. -I/usr/src/cmd/awk
all: awk
awk: b.b main.b token.b tran.b lib.b run.b parse.b y.tab.b lex.yy.b proctab.b ../libm/libm.a
	$(CC) -i -s b.b main.b token.b tran.b lib.b run.b parse.b y.tab.b lex.yy.b proctab.b ../libm/libm.a -o awk
y.tab.c: /usr/src/cmd/awk/awk.g.y /bin/yacc
	/bin/yacc -d /usr/src/cmd/awk/awk.g.y
	/bin/cp y.tab.h awk.h
lex.yy.c: /usr/src/cmd/awk/awk.lx.l ../lex/lex
	../lex/lex /usr/src/cmd/awk/awk.lx.l
token.c: y.tab.c /usr/src/cmd/awk/token.c /usr/src/cmd/awk/tokenscript
	/bin/cp /usr/src/cmd/awk/token.c token.c
	/bin/ed - < /usr/src/cmd/awk/tokenscript
proc.b: /usr/src/cmd/awk/proc.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/awk/proc.c
awkproc: proc.b token.b
	$(CC) -i proc.b token.b -o awkproc
runproc: ../normal.c
	$(CC) -i ../normal.c -o runproc
proctab.c: awkproc runproc
	./runproc ./awkproc > proctab.c
b.b: /usr/src/cmd/awk/b.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/awk/b.c
main.b: /usr/src/cmd/awk/main.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/awk/main.c
token.b: token.c y.tab.c
	$(CC) $(CFLAGS) -c token.c
tran.b: /usr/src/cmd/awk/tran.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/awk/tran.c
lib.b: /usr/src/cmd/awk/lib.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/awk/lib.c
run.b: /usr/src/cmd/awk/run.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/awk/run.c
parse.b: /usr/src/cmd/awk/parse.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/awk/parse.c
y.tab.b: y.tab.c
	$(CC) $(CFLAGS) -c y.tab.c
lex.yy.b: lex.yy.c y.tab.c
	$(CC) $(CFLAGS) -c lex.yy.c
proctab.b: proctab.c y.tab.c
	$(CC) $(CFLAGS) -c proctab.c
install: all
	/bin/cp awk /bin/ninstall
	/bin/mv /bin/ninstall /bin/awk </dev/null
clean:
	/bin/rm -f *.b awk y.tab.c lex.yy.c proctab.c token.c runproc awkproc
