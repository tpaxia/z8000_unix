CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002
all: libt4014.a
libt4014.a: arc.b circle.b close.b dot.b erase.b label.b line.b linemod.b move.b open.b point.b scale.b space.b subr.b box.b
	/bin/rm -f libt4014.a
	/bin/ar qc libt4014.a arc.b circle.b close.b dot.b erase.b label.b line.b linemod.b move.b open.b point.b scale.b space.b subr.b box.b
arc.b: /usr/src/libplot/t4014/arc.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t4014/arc.c
circle.b: /usr/src/libplot/t4014/circle.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t4014/circle.c
close.b: /usr/src/libplot/t4014/close.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t4014/close.c
dot.b: /usr/src/libplot/t4014/dot.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t4014/dot.c
erase.b: /usr/src/libplot/t4014/erase.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t4014/erase.c
label.b: /usr/src/libplot/t4014/label.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t4014/label.c
line.b: /usr/src/libplot/t4014/line.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t4014/line.c
linemod.b: /usr/src/libplot/t4014/linemod.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t4014/linemod.c
move.b: /usr/src/libplot/t4014/move.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t4014/move.c
open.b: /usr/src/libplot/t4014/open.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t4014/open.c
point.b: /usr/src/libplot/t4014/point.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t4014/point.c
scale.b: /usr/src/libplot/t4014/scale.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t4014/scale.c
space.b: /usr/src/libplot/t4014/space.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t4014/space.c
subr.b: /usr/src/libplot/t4014/subr.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t4014/subr.c
box.b: /usr/src/libplot/t4014/box.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t4014/box.c
install: all
	/bin/cp libt4014.a /lib/libt4014.a
clean:
	/bin/rm -f *.b libt4014.a
