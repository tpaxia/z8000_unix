CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002
all: libt300.a
libt300.a: arc.b circle.b close.b dot.b erase.b label.b line.b linmod.b move.b open.b point.b space.b subr.b box.b
	/bin/rm -f libt300.a
	/bin/ar qc libt300.a arc.b circle.b close.b dot.b erase.b label.b line.b linmod.b move.b open.b point.b space.b subr.b box.b
arc.b: /usr/src/libplot/t300/arc.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t300/arc.c
circle.b: /usr/src/libplot/t300/circle.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t300/circle.c
close.b: /usr/src/libplot/t300/close.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t300/close.c
dot.b: /usr/src/libplot/t300/dot.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t300/dot.c
erase.b: /usr/src/libplot/t300/erase.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t300/erase.c
label.b: /usr/src/libplot/t300/label.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t300/label.c
line.b: /usr/src/libplot/t300/line.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t300/line.c
linmod.b: /usr/src/libplot/t300/linmod.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t300/linmod.c
move.b: /usr/src/libplot/t300/move.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t300/move.c
open.b: /usr/src/libplot/t300/open.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t300/open.c
point.b: /usr/src/libplot/t300/point.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t300/point.c
space.b: /usr/src/libplot/t300/space.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t300/space.c
subr.b: /usr/src/libplot/t300/subr.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t300/subr.c
box.b: /usr/src/libplot/t300/box.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t300/box.c
install: all
	/bin/cp libt300.a /lib/libt300.a
clean:
	/bin/rm -f *.b libt300.a
