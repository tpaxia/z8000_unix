CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: file
file: file.b
	$(CC) -i -s file.b -o file
file.b: /usr/src/cmd/file.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/file.c
install: all
	/bin/cp file /bin/ninstall
	/bin/mv /bin/ninstall /bin/file </dev/null
clean:
	/bin/rm -f *.b file
