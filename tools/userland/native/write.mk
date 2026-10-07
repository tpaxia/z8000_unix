CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: write
write: write.b
	$(CC) -i -s write.b -o write
write.b: /usr/src/cmd/write.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/write.c
install: all
	/bin/cp write /bin/ninstall
	/bin/mv /bin/ninstall /bin/write </dev/null
clean:
	/bin/rm -f *.b write
