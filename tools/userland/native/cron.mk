CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: cron
cron: cron.b
	$(CC) -i -s cron.b -o cron
cron.b: /usr/src/cmd/cron.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/cron.c
install: all
	/bin/cp cron /bin/ninstall
	/bin/mv /bin/ninstall /bin/cron </dev/null
clean:
	/bin/rm -f *.b cron
