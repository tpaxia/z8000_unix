CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: df
df: df.b
	$(CC) -i -s df.b -o df
df.b: /usr/src/cmd/df.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/df.c
install: all
	/bin/cp df /bin/ninstall
	/bin/mv /bin/ninstall /bin/df </dev/null
clean:
	/bin/rm -f *.b df
