CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: vplot
vplot: vplot.b chrtab.b
	$(CC) -i -s vplot.b chrtab.b -o vplot
vplot.b: /usr/src/cmd/plot/vplot.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/plot/vplot.c
chrtab.b: /usr/src/cmd/plot/chrtab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/plot/chrtab.c
install: all
	/bin/cp vplot /bin/ninstall
	/bin/mv /bin/ninstall /bin/vplot </dev/null
clean:
	/bin/rm -f *.b vplot
