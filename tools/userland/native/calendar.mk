CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: calendar
calendar: calendar.b
	$(CC) -i -s calendar.b -o calendar
calendar.b: /usr/src/cmd/calendar.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/calendar.c
install: all
	/bin/cp calendar /usr/lib/ninstall
	/bin/mv /usr/lib/ninstall /usr/lib/calendar </dev/null
clean:
	/bin/rm -f *.b calendar
