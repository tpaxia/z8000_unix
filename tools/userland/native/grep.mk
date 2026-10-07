CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: grep
grep: grep.b
	$(CC) -i -s grep.b -o grep
grep.b: /usr/src/cmd/grep.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/grep.c
install: all
	/bin/cp grep /bin/ninstall
	/bin/mv /bin/ninstall /bin/grep </dev/null
clean:
	/bin/rm -f *.b grep
