CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: uuclean
uuclean: uuclean.b gnamef.b prefix.b sdmail.b getpwinfo.b
	$(CC) -i -s uuclean.b gnamef.b prefix.b sdmail.b getpwinfo.b -o uuclean
uuclean.b: /usr/src/cmd/uucp/uuclean.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/uuclean.c
gnamef.b: /usr/src/cmd/uucp/gnamef.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/gnamef.c
prefix.b: /usr/src/cmd/uucp/prefix.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/prefix.c
sdmail.b: /usr/src/cmd/uucp/sdmail.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/sdmail.c
getpwinfo.b: /usr/src/cmd/uucp/getpwinfo.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/uucp/getpwinfo.c
install: all
	/bin/cp uuclean /bin/ninstall
	/bin/mv /bin/ninstall /bin/uuclean </dev/null
clean:
	/bin/rm -f *.b uuclean
