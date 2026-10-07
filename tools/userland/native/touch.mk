CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: touch
touch: touch.b
	$(CC) -i -s touch.b -o touch
touch.b: /usr/src/cmd/touch.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/touch.c
install: all
	/bin/cp touch /bin/ninstall
	/bin/mv /bin/ninstall /bin/touch </dev/null
clean:
	/bin/rm -f *.b touch
