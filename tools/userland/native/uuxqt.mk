CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: uuxqt
uuxqt: uuxqt.b cpmv.b expfile.b gename.b getpwinfo.b index.b lastpart.b prefix.b shio.b ulockf.b xqt.b getprm.b gnamef.b logent.b
	$(CC) -i -s uuxqt.b cpmv.b expfile.b gename.b getpwinfo.b index.b lastpart.b prefix.b shio.b ulockf.b xqt.b getprm.b gnamef.b logent.b -o uuxqt
uuxqt.b: /usr/src/cmd/uucp/uuxqt.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/uuxqt.c
cpmv.b: /usr/src/cmd/uucp/cpmv.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/cpmv.c
expfile.b: /usr/src/cmd/uucp/expfile.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/expfile.c
gename.b: /usr/src/cmd/uucp/gename.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/gename.c
getpwinfo.b: /usr/src/cmd/uucp/getpwinfo.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/getpwinfo.c
index.b: /usr/src/cmd/uucp/index.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/index.c
lastpart.b: /usr/src/cmd/uucp/lastpart.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/lastpart.c
prefix.b: /usr/src/cmd/uucp/prefix.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/prefix.c
shio.b: /usr/src/cmd/uucp/shio.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/shio.c
ulockf.b: /usr/src/cmd/uucp/ulockf.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/ulockf.c
xqt.b: /usr/src/cmd/uucp/xqt.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/xqt.c
getprm.b: /usr/src/cmd/uucp/getprm.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/getprm.c
gnamef.b: /usr/src/cmd/uucp/gnamef.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/gnamef.c
logent.b: /usr/src/cmd/uucp/logent.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/logent.c
install: all
	/bin/cp uuxqt /bin/ninstall
	/bin/mv /bin/ninstall /bin/uuxqt </dev/null
clean:
	/bin/rm -f *.b uuxqt
