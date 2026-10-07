CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: echo
echo: echo.b
	$(CC) -i -s echo.b -o echo
echo.b: /usr/src/cmd/echo.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/echo.c
install: all
	/bin/cp echo /bin/ninstall
	/bin/mv /bin/ninstall /bin/echo </dev/null
clean:
	/bin/rm -f *.b echo
