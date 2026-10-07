CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: spline
spline: spline.b ../libm/libm.a
	$(CC) -i -s spline.b ../libm/libm.a -o spline
spline.b: /usr/src/cmd/spline.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/spline.c
install: all
	/bin/cp spline /bin/ninstall
	/bin/mv /bin/ninstall /bin/spline </dev/null
clean:
	/bin/rm -f *.b spline
