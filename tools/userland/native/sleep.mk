CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: sleep
sleep: sleep.b
	$(CC) -i -s sleep.b -o sleep
sleep.b: /usr/src/cmd/sleep.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/sleep.c
install: all
	/bin/cp sleep /bin/ninstall
	/bin/mv /bin/ninstall /bin/sleep </dev/null
clean:
	/bin/rm -f *.b sleep
