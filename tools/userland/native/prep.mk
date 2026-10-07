CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: prep
prep: prep0.b prep1.b prep2.b
	$(CC) -i -s prep0.b prep1.b prep2.b -o prep
prep0.b: /usr/src/cmd/prep/prep0.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/prep/prep0.c
prep1.b: /usr/src/cmd/prep/prep1.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/prep/prep1.c
prep2.b: /usr/src/cmd/prep/prep2.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/prep/prep2.c
install: all
	/bin/cp prep /bin/ninstall
	/bin/mv /bin/ninstall /bin/prep </dev/null
clean:
	/bin/rm -f *.b prep
