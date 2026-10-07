CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: date
date: date.b
	$(CC) -i -s date.b -o date
date.b: /usr/src/cmd/date.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/date.c
install: all
	/bin/cp date /bin/ninstall
	/bin/mv /bin/ninstall /bin/date </dev/null
clean:
	/bin/rm -f *.b date
