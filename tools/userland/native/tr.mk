CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: tr
tr: tr.b
	$(CC) -i -s tr.b -o tr
tr.b: /usr/src/cmd/tr.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tr.c
install: all
	/bin/cp tr /bin/ninstall
	/bin/mv /bin/ninstall /bin/tr </dev/null
clean:
	/bin/rm -f *.b tr
