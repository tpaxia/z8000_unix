CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I/usr/src/cmd
all: primes
primes: primes.b num56.b
	$(CC) -i -s primes.b num56.b -o primes
primes.b: /usr/src/cmd/primes.c /usr/src/cmd/num56.h
	$(CC) $(CFLAGS) -c /usr/src/cmd/primes.c
num56.b: /usr/src/cmd/num56.az8
	/bin/asz8k -zc -o num56.b /usr/src/cmd/num56.az8
install: all
	/bin/cp primes /bin/primes
clean:
	/bin/rm -f *.b primes
