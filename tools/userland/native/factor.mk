CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I/usr/src/cmd
all: factor
factor: factor.b num56.b
	$(CC) -i -s factor.b num56.b -o factor
factor.b: /usr/src/cmd/factor.c /usr/src/cmd/num56.h
	$(CC) $(CFLAGS) -c /usr/src/cmd/factor.c
num56.b: /usr/src/cmd/num56.az8
	/bin/asz8k -zc -o num56.b /usr/src/cmd/num56.az8
install: all
	/bin/cp factor /bin/factor
clean:
	/bin/rm -f *.b factor
