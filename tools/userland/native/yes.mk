CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: yes
yes: yes.b
	$(CC) -i -s yes.b -o yes
yes.b: /usr/src/cmd/yes.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/yes.c
install: all
	/bin/cp yes /bin/ninstall
	/bin/mv /bin/ninstall /bin/yes </dev/null
clean:
	/bin/rm -f *.b yes
