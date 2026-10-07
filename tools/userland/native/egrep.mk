CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I. -I/usr/src/cmd
all: egrep
egrep: y.tab.b
	$(CC) -i -s y.tab.b -o egrep
y.tab.c: /usr/src/cmd/egrep.y /bin/yacc
	/bin/yacc -d /usr/src/cmd/egrep.y
y.tab.b: y.tab.c
	$(CC) $(CFLAGS) -c y.tab.c
install: all
	/bin/cp egrep /bin/ninstall
	/bin/mv /bin/ninstall /bin/egrep </dev/null
clean:
	/bin/rm -f *.b egrep y.tab.c
