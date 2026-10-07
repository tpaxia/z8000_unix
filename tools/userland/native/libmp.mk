CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002
all: libmp.a
libmp.a: pow.b gcd.b msqrt.b mult.b mdiv.b mout.b madd.b util.b
	/bin/rm -f libmp.a
	/bin/ar qc libmp.a pow.b gcd.b msqrt.b mult.b mdiv.b mout.b madd.b util.b
pow.b: /usr/src/libmp/pow.c
	$(CC) $(CFLAGS) -c /usr/src/libmp/pow.c
gcd.b: /usr/src/libmp/gcd.c
	$(CC) $(CFLAGS) -c /usr/src/libmp/gcd.c
msqrt.b: /usr/src/libmp/msqrt.c
	$(CC) $(CFLAGS) -c /usr/src/libmp/msqrt.c
mult.b: /usr/src/libmp/mult.c
	$(CC) $(CFLAGS) -c /usr/src/libmp/mult.c
mdiv.b: /usr/src/libmp/mdiv.c
	$(CC) $(CFLAGS) -c /usr/src/libmp/mdiv.c
mout.b: /usr/src/libmp/mout.c
	$(CC) $(CFLAGS) -c /usr/src/libmp/mout.c
madd.b: /usr/src/libmp/madd.c
	$(CC) $(CFLAGS) -c /usr/src/libmp/madd.c
util.b: /usr/src/libmp/util.c
	$(CC) $(CFLAGS) -c /usr/src/libmp/util.c
install: all
	/bin/cp libmp.a /lib/libmp.a
clean:
	/bin/rm -f *.b libmp.a
