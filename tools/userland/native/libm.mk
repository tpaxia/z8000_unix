CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002
all: libm.a
libm.a: asin.b atan.b hypot.b jn.b j0.b j1.b pow.b fabs.b log.b sin.b sqrt.b tan.b tanh.b sinh.b exp.b floor.b
	/bin/rm -f libm.a
	/bin/ar qc libm.a asin.b atan.b hypot.b jn.b j0.b j1.b pow.b fabs.b log.b sin.b sqrt.b tan.b tanh.b sinh.b exp.b
	/bin/ar qc libm.a floor.b
asin.b: /usr/src/libm/asin.c
	$(CC) $(CFLAGS) -c /usr/src/libm/asin.c
atan.b: /usr/src/libm/atan.c
	$(CC) $(CFLAGS) -c /usr/src/libm/atan.c
hypot.b: /usr/src/libm/hypot.c
	$(CC) $(CFLAGS) -c /usr/src/libm/hypot.c
jn.b: /usr/src/libm/jn.c
	$(CC) $(CFLAGS) -c /usr/src/libm/jn.c
j0.b: /usr/src/libm/j0.c
	$(CC) $(CFLAGS) -c /usr/src/libm/j0.c
j1.b: /usr/src/libm/j1.c
	$(CC) $(CFLAGS) -c /usr/src/libm/j1.c
pow.b: /usr/src/libm/pow.c
	$(CC) $(CFLAGS) -c /usr/src/libm/pow.c
fabs.b: /usr/src/libm/fabs.c
	$(CC) $(CFLAGS) -c /usr/src/libm/fabs.c
log.b: /usr/src/libm/log.c
	$(CC) $(CFLAGS) -c /usr/src/libm/log.c
sin.b: /usr/src/libm/sin.c
	$(CC) $(CFLAGS) -c /usr/src/libm/sin.c
sqrt.b: /usr/src/libm/sqrt.c
	$(CC) $(CFLAGS) -c /usr/src/libm/sqrt.c
tan.b: /usr/src/libm/tan.c
	$(CC) $(CFLAGS) -c /usr/src/libm/tan.c
tanh.b: /usr/src/libm/tanh.c
	$(CC) $(CFLAGS) -c /usr/src/libm/tanh.c
sinh.b: /usr/src/libm/sinh.c
	$(CC) $(CFLAGS) -c /usr/src/libm/sinh.c
exp.b: /usr/src/libm/exp.c
	$(CC) $(CFLAGS) -c /usr/src/libm/exp.c
floor.b: /usr/src/libm/floor.c
	$(CC) $(CFLAGS) -c /usr/src/libm/floor.c
install: all
	/bin/cp libm.a /lib/libm.a
clean:
	/bin/rm -f *.b libm.a
