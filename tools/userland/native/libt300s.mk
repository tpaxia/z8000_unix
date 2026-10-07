CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002
all: libt300s.a
libt300s.a: arc.b circle.b close.b dot.b erase.b label.b line.b linmod.b move.b open.b point.b space.b subr.b box.b
	/bin/rm -f libt300s.a
	/bin/ar qc libt300s.a arc.b circle.b close.b dot.b erase.b label.b line.b linmod.b move.b open.b point.b space.b subr.b box.b
arc.b: /usr/src/libplot/t300s/arc.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t300s/arc.c
circle.b: /usr/src/libplot/t300s/circle.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t300s/circle.c
close.b: /usr/src/libplot/t300s/close.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t300s/close.c
dot.b: /usr/src/libplot/t300s/dot.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t300s/dot.c
erase.b: /usr/src/libplot/t300s/erase.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t300s/erase.c
label.b: /usr/src/libplot/t300s/label.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t300s/label.c
line.b: /usr/src/libplot/t300s/line.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t300s/line.c
linmod.b: /usr/src/libplot/t300s/linmod.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t300s/linmod.c
move.b: /usr/src/libplot/t300s/move.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t300s/move.c
open.b: /usr/src/libplot/t300s/open.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t300s/open.c
point.b: /usr/src/libplot/t300s/point.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t300s/point.c
space.b: /usr/src/libplot/t300s/space.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t300s/space.c
subr.b: /usr/src/libplot/t300s/subr.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t300s/subr.c
box.b: /usr/src/libplot/t300s/box.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t300s/box.c
install: all
	/bin/cp libt300s.a /lib/libt300s.a
clean:
	/bin/rm -f *.b libt300s.a
