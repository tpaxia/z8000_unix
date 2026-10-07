CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002
all: libvt0.a
libvt0.a: arc.b circle.b close.b dot.b erase.b frame.b label.b line.b move.b open.b point.b space.b subr.b linmod.b box.b
	/bin/rm -f libvt0.a
	/bin/ar qc libvt0.a arc.b circle.b close.b dot.b erase.b frame.b label.b line.b move.b open.b point.b space.b subr.b linmod.b box.b
arc.b: /usr/src/libplot/vt0/arc.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/vt0/arc.c
circle.b: /usr/src/libplot/vt0/circle.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/vt0/circle.c
close.b: /usr/src/libplot/vt0/close.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/vt0/close.c
dot.b: /usr/src/libplot/vt0/dot.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/vt0/dot.c
erase.b: /usr/src/libplot/vt0/erase.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/vt0/erase.c
frame.b: /usr/src/libplot/vt0/frame.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/vt0/frame.c
label.b: /usr/src/libplot/vt0/label.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/vt0/label.c
line.b: /usr/src/libplot/vt0/line.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/vt0/line.c
move.b: /usr/src/libplot/vt0/move.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/vt0/move.c
open.b: /usr/src/libplot/vt0/open.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/vt0/open.c
point.b: /usr/src/libplot/vt0/point.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/vt0/point.c
space.b: /usr/src/libplot/vt0/space.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/vt0/space.c
subr.b: /usr/src/libplot/vt0/subr.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/vt0/subr.c
linmod.b: /usr/src/libplot/vt0/linmod.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/vt0/linmod.c
box.b: /usr/src/libplot/vt0/box.c
	$(CC) $(CFLAGS) -c /usr/src/libplot/vt0/box.c
install: all
	/bin/cp libvt0.a /lib/libvt0.a
clean:
	/bin/rm -f *.b libvt0.a
