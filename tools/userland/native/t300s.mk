CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: t300s
t300s: driver.b ../libt300s/libt300s.a ../libm/libm.a
	$(CC) -i -s driver.b ../libt300s/libt300s.a ../libm/libm.a -o t300s
driver.b: /usr/src/cmd/plot/driver.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/plot/driver.c
install: all
	/bin/cp t300s /bin/ninstall
	/bin/mv /bin/ninstall /bin/t300s </dev/null
clean:
	/bin/rm -f *.b t300s
