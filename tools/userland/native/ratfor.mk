CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I. -I/usr/src/cmd/ratfor
all: ratfor
ratfor: r0.b r1.b r2.b rio.b rlook.b rlex.b y.tab.b
	$(CC) -i -s r0.b r1.b r2.b rio.b rlook.b rlex.b y.tab.b -o ratfor
y.tab.c: /usr/src/cmd/ratfor/r.g /bin/yacc
	/bin/yacc -d /usr/src/cmd/ratfor/r.g
r0.b: /usr/src/cmd/ratfor/r0.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/ratfor/r0.c
r1.b: /usr/src/cmd/ratfor/r1.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/ratfor/r1.c
r2.b: /usr/src/cmd/ratfor/r2.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/ratfor/r2.c
rio.b: /usr/src/cmd/ratfor/rio.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/ratfor/rio.c
rlook.b: /usr/src/cmd/ratfor/rlook.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/ratfor/rlook.c
rlex.b: /usr/src/cmd/ratfor/rlex.c y.tab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/ratfor/rlex.c
y.tab.b: y.tab.c
	$(CC) $(CFLAGS) -c y.tab.c
install: all
	/bin/cp ratfor /bin/ninstall
	/bin/mv /bin/ninstall /bin/ratfor </dev/null
clean:
	/bin/rm -f *.b ratfor y.tab.c
