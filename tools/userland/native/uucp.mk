CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: uucp
uucp: uucp.b gwd.b cpmv.b expfile.b gename.b getpwinfo.b index.b lastpart.b prefix.b shio.b ulockf.b xqt.b chkpth.b getargs.b logent.b versys.b
	$(CC) -i -s uucp.b gwd.b cpmv.b expfile.b gename.b getpwinfo.b index.b lastpart.b prefix.b shio.b ulockf.b xqt.b chkpth.b getargs.b logent.b versys.b -o uucp
uucp.b: /usr/src/cmd/uucp/uucp.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/uucp.c
gwd.b: /usr/src/cmd/uucp/gwd.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/gwd.c
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
chkpth.b: /usr/src/cmd/uucp/chkpth.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/chkpth.c
getargs.b: /usr/src/cmd/uucp/getargs.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/getargs.c
logent.b: /usr/src/cmd/uucp/logent.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/logent.c
versys.b: /usr/src/cmd/uucp/versys.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/versys.c
install: all
	/bin/cp uucp /bin/ninstall
	/bin/mv /bin/ninstall /bin/uucp </dev/null
clean:
	/bin/rm -f *.b uucp
