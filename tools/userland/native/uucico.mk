CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: uucico
uucico: cico.b cntrl.b conn.b pk0.b pk1.b gio.b sdmail.b pkon.b cpmv.b expfile.b gename.b getpwinfo.b index.b lastpart.b prefix.b shio.b ulockf.b xqt.b anlwrk.b chkpth.b getargs.b gnamef.b gnsys.b gnxseq.b imsg.b logent.b sysacct.b systat.b
	$(CC) -i -s cico.b cntrl.b conn.b pk0.b pk1.b gio.b sdmail.b pkon.b cpmv.b expfile.b gename.b getpwinfo.b index.b lastpart.b prefix.b shio.b ulockf.b xqt.b anlwrk.b chkpth.b getargs.b gnamef.b gnsys.b gnxseq.b imsg.b logent.b sysacct.b systat.b -o uucico
cico.b: /usr/src/cmd/uucp/cico.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/cico.c
cntrl.b: /usr/src/cmd/uucp/cntrl.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/cntrl.c
conn.b: /usr/src/cmd/uucp/conn.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/conn.c
pk0.b: /usr/src/cmd/uucp/pk0.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/pk0.c
pk1.b: /usr/src/cmd/uucp/pk1.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/pk1.c
gio.b: /usr/src/cmd/uucp/gio.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/gio.c
sdmail.b: /usr/src/cmd/uucp/sdmail.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/sdmail.c
pkon.b: /usr/src/cmd/uucp/pkon.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/pkon.c
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
anlwrk.b: /usr/src/cmd/uucp/anlwrk.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/anlwrk.c
chkpth.b: /usr/src/cmd/uucp/chkpth.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/chkpth.c
getargs.b: /usr/src/cmd/uucp/getargs.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/getargs.c
gnamef.b: /usr/src/cmd/uucp/gnamef.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/gnamef.c
gnsys.b: /usr/src/cmd/uucp/gnsys.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/gnsys.c
gnxseq.b: /usr/src/cmd/uucp/gnxseq.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/gnxseq.c
imsg.b: /usr/src/cmd/uucp/imsg.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/imsg.c
logent.b: /usr/src/cmd/uucp/logent.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/logent.c
sysacct.b: /usr/src/cmd/uucp/sysacct.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/sysacct.c
systat.b: /usr/src/cmd/uucp/systat.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/systat.c
install: all
	/bin/cp uucico /bin/ninstall
	/bin/mv /bin/ninstall /bin/uucico </dev/null
clean:
	/bin/rm -f *.b uucico
