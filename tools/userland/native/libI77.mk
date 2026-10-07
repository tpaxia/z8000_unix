CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -DSYLMX=300
all: libI77.a
libI77.a: backspace.b dfe.b due.b iio.b inquire.b lib.b rewind.b rsfe.b rdfmt.b sue.b uio.b wsfe.b sfe.b fmt.b lio.b lread.b open.b close.b util.b endfile.b wrtfmt.b err.b fmtlib.b dballoc.b
	/bin/rm -f libI77.a
	/bin/ar qc libI77.a backspace.b dfe.b due.b iio.b inquire.b lib.b rewind.b rsfe.b rdfmt.b sue.b uio.b wsfe.b sfe.b fmt.b lio.b
	/bin/ar qc libI77.a lread.b open.b close.b util.b endfile.b wrtfmt.b err.b fmtlib.b dballoc.b
backspace.b: /usr/src/libI77/backspace.c
	$(CC) $(CFLAGS) -c /usr/src/libI77/backspace.c
dfe.b: /usr/src/libI77/dfe.c
	$(CC) $(CFLAGS) -c /usr/src/libI77/dfe.c
due.b: /usr/src/libI77/due.c
	$(CC) $(CFLAGS) -c /usr/src/libI77/due.c
iio.b: /usr/src/libI77/iio.c
	$(CC) $(CFLAGS) -c /usr/src/libI77/iio.c
inquire.b: /usr/src/libI77/inquire.c
	$(CC) $(CFLAGS) -c /usr/src/libI77/inquire.c
lib.b: /usr/src/libI77/lib.c
	$(CC) $(CFLAGS) -c /usr/src/libI77/lib.c
rewind.b: /usr/src/libI77/rewind.c
	$(CC) $(CFLAGS) -c /usr/src/libI77/rewind.c
rsfe.b: /usr/src/libI77/rsfe.c
	$(CC) $(CFLAGS) -c /usr/src/libI77/rsfe.c
rdfmt.b: /usr/src/libI77/rdfmt.c
	$(CC) $(CFLAGS) -c /usr/src/libI77/rdfmt.c
sue.b: /usr/src/libI77/sue.c
	$(CC) $(CFLAGS) -c /usr/src/libI77/sue.c
uio.b: /usr/src/libI77/uio.c
	$(CC) $(CFLAGS) -c /usr/src/libI77/uio.c
wsfe.b: /usr/src/libI77/wsfe.c
	$(CC) $(CFLAGS) -c /usr/src/libI77/wsfe.c
sfe.b: /usr/src/libI77/sfe.c
	$(CC) $(CFLAGS) -c /usr/src/libI77/sfe.c
fmt.b: /usr/src/libI77/fmt.c
	$(CC) $(CFLAGS) -c /usr/src/libI77/fmt.c
lio.b: /usr/src/libI77/lio.c
	$(CC) $(CFLAGS) -c /usr/src/libI77/lio.c
lread.b: /usr/src/libI77/lread.c
	$(CC) $(CFLAGS) -c /usr/src/libI77/lread.c
open.b: /usr/src/libI77/open.c
	$(CC) $(CFLAGS) -c /usr/src/libI77/open.c
close.b: /usr/src/libI77/close.c
	$(CC) $(CFLAGS) -c /usr/src/libI77/close.c
util.b: /usr/src/libI77/util.c
	$(CC) $(CFLAGS) -c /usr/src/libI77/util.c
endfile.b: /usr/src/libI77/endfile.c
	$(CC) $(CFLAGS) -c /usr/src/libI77/endfile.c
wrtfmt.b: /usr/src/libI77/wrtfmt.c
	$(CC) $(CFLAGS) -c /usr/src/libI77/wrtfmt.c
err.b: /usr/src/libI77/err.c
	$(CC) $(CFLAGS) -c /usr/src/libI77/err.c
fmtlib.b: /usr/src/libI77/fmtlib.c
	$(CC) $(CFLAGS) -c /usr/src/libI77/fmtlib.c
dballoc.b: /usr/src/libI77/dballoc.c
	$(CC) $(CFLAGS) -c /usr/src/libI77/dballoc.c
install: all
	/bin/cp libI77.a /lib/libI77.a
clean:
	/bin/rm -f *.b libI77.a
