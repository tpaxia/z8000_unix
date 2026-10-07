CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: graph
graph: graph.b ../libplot/libplot.a ../libm/libm.a
	$(CC) -i -s graph.b ../libplot/libplot.a ../libm/libm.a -o graph
graph.b: /usr/src/cmd/graph.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/graph.c
install: all
	/bin/cp graph /bin/ninstall
	/bin/mv /bin/ninstall /bin/graph </dev/null
clean:
	/bin/rm -f *.b graph
