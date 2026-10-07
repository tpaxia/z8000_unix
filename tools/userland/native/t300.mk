CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: t300
t300: driver.b ../libt300/libt300.a ../libm/libm.a
	$(CC) -i -s driver.b ../libt300/libt300.a ../libm/libm.a -o t300
driver.b: /usr/src/cmd/plot/driver.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/plot/driver.c
install: all
	/bin/cp t300 /bin/ninstall
	/bin/mv /bin/ninstall /bin/t300 </dev/null
clean:
	/bin/rm -f *.b t300
