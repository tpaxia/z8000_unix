CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: cal
cal: cal.b
	$(CC) -i -s cal.b -o cal
cal.b: /usr/src/cmd/cal.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/cal.c
install: all
	/bin/cp cal /bin/ninstall
	/bin/mv /bin/ninstall /bin/cal </dev/null
clean:
	/bin/rm -f *.b cal
