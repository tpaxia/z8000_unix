CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: login
login: login.b
	$(CC) -i -s login.b -o login
login.b: /usr/src/cmd/login.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/login.c
install: all
	/bin/cp login /bin/ninstall
	/bin/mv /bin/ninstall /bin/login </dev/null
clean:
	/bin/rm -f *.b login
