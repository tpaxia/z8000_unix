CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I. -I/usr/src/cmd
all: expr
expr: y.tab.b
	$(CC) -i -s y.tab.b -o expr
y.tab.c: /usr/src/cmd/expr.y /bin/yacc
	/bin/yacc -d /usr/src/cmd/expr.y
y.tab.b: y.tab.c
	$(CC) $(CFLAGS) -c y.tab.c
install: all
	/bin/cp expr /bin/ninstall
	/bin/mv /bin/ninstall /bin/expr </dev/null
clean:
	/bin/rm -f *.b expr y.tab.c
