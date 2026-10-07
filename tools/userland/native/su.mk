CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: su
su: su.b
	$(CC) -i -s su.b -o su
su.b: /usr/src/cmd/su.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/su.c
install: all
	/bin/cp su /bin/ninstall
	/bin/mv /bin/ninstall /bin/su </dev/null
clean:
	/bin/rm -f *.b su
