CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: checkeq
checkeq: checkeq.b
	$(CC) -i -s checkeq.b -o checkeq
checkeq.b: /usr/src/cmd/checkeq.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/checkeq.c
install: all
	/bin/cp checkeq /bin/ninstall
	/bin/mv /bin/ninstall /bin/checkeq </dev/null
clean:
	/bin/rm -f *.b checkeq
