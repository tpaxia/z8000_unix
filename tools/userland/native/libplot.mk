CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002
all: libplot.a
libplot.a: arc.b circle.b close.b cont.b dot.b erase.b label.b line.b linmod.b move.b open.b point.b putsi.b space.b box.b
	/bin/rm -f libplot.a
	/bin/ar qc libplot.a arc.b circle.b close.b cont.b dot.b erase.b label.b line.b linmod.b move.b open.b point.b putsi.b space.b box.b
arc.b: /usr/src/libplot/plot/arc.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/plot/arc.c
circle.b: /usr/src/libplot/plot/circle.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/plot/circle.c
close.b: /usr/src/libplot/plot/close.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/plot/close.c
cont.b: /usr/src/libplot/plot/cont.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/plot/cont.c
dot.b: /usr/src/libplot/plot/dot.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/plot/dot.c
erase.b: /usr/src/libplot/plot/erase.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/plot/erase.c
label.b: /usr/src/libplot/plot/label.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/plot/label.c
line.b: /usr/src/libplot/plot/line.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/plot/line.c
linmod.b: /usr/src/libplot/plot/linmod.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/plot/linmod.c
move.b: /usr/src/libplot/plot/move.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/plot/move.c
open.b: /usr/src/libplot/plot/open.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/plot/open.c
point.b: /usr/src/libplot/plot/point.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/plot/point.c
putsi.b: /usr/src/libplot/plot/putsi.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/plot/putsi.c
space.b: /usr/src/libplot/plot/space.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/plot/space.c
box.b: /usr/src/libplot/plot/box.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/plot/box.c
install: all
	/bin/cp libplot.a /lib/libplot.a
clean:
	/bin/rm -f *.b libplot.a
