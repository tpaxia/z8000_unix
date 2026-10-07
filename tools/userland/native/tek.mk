CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: tek
tek: driver.b ../libt4014/libt4014.a ../libm/libm.a
	$(CC) -i -s driver.b ../libt4014/libt4014.a ../libm/libm.a -o tek
driver.b: /usr/src/cmd/plot/driver.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/plot/driver.c
install: all
	/bin/cp tek /bin/ninstall
	/bin/mv /bin/ninstall /bin/tek </dev/null
clean:
	/bin/rm -f *.b tek
