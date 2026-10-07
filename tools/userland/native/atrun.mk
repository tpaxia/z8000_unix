CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: atrun
atrun: atrun.b
	$(CC) -i -s atrun.b -o atrun
atrun.b: /usr/src/cmd/atrun.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/atrun.c
install: all
	/bin/cp atrun /bin/ninstall
	/bin/mv /bin/ninstall /bin/atrun </dev/null
clean:
	/bin/rm -f *.b atrun
