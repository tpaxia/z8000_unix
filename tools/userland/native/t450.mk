CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: t450
t450: driver.b ../libt450/libt450.a ../libm/libm.a
	$(CC) -i -s driver.b ../libt450/libt450.a ../libm/libm.a -o t450
driver.b: /usr/src/cmd/plot/driver.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/plot/driver.c
install: all
	/bin/cp t450 /bin/ninstall
	/bin/mv /bin/ninstall /bin/t450 </dev/null
clean:
	/bin/rm -f *.b t450
