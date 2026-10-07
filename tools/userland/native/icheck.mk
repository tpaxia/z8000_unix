CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: icheck
icheck: icheck.b
	$(CC) -i -s icheck.b -o icheck
icheck.b: /usr/src/cmd/icheck.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/icheck.c
install: all
	/bin/cp icheck /bin/ninstall
	/bin/mv /bin/ninstall /bin/icheck </dev/null
clean:
	/bin/rm -f *.b icheck
