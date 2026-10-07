CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: dmesg
dmesg: dmesg.b
	$(CC) -i -s dmesg.b -o dmesg
dmesg.b: /usr/src/cmd/dmesg.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/dmesg.c
install: all
clean:
	/bin/rm -f *.b dmesg
