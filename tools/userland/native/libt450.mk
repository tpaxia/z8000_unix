CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002
all: libt450.a
libt450.a: arc.b circle.b close.b dot.b erase.b label.b line.b linmod.b move.b open.b point.b space.b subr.b box.b
	/bin/rm -f libt450.a
	/bin/ar qc libt450.a arc.b circle.b close.b dot.b erase.b label.b line.b linmod.b move.b open.b point.b space.b subr.b box.b
arc.b: /usr/src/libplot/t450/arc.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t450/arc.c
circle.b: /usr/src/libplot/t450/circle.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t450/circle.c
close.b: /usr/src/libplot/t450/close.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t450/close.c
dot.b: /usr/src/libplot/t450/dot.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t450/dot.c
erase.b: /usr/src/libplot/t450/erase.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t450/erase.c
label.b: /usr/src/libplot/t450/label.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t450/label.c
line.b: /usr/src/libplot/t450/line.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t450/line.c
linmod.b: /usr/src/libplot/t450/linmod.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t450/linmod.c
move.b: /usr/src/libplot/t450/move.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t450/move.c
open.b: /usr/src/libplot/t450/open.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t450/open.c
point.b: /usr/src/libplot/t450/point.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t450/point.c
space.b: /usr/src/libplot/t450/space.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t450/space.c
subr.b: /usr/src/libplot/t450/subr.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t450/subr.c
box.b: /usr/src/libplot/t450/box.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/t450/box.c
install: all
	/bin/cp libt450.a /lib/libt450.a
clean:
	/bin/rm -f *.b libt450.a
