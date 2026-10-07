CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: hangman
hangman: hangman.b
	$(CC) -i -s hangman.b -o hangman
hangman.b: /usr/src/games/hangman.c
	$(CC) $(CFLAGS) -c /usr/src/games/hangman.c
install: all
	/bin/cp hangman /usr/games/ninstall
	/bin/mv /usr/games/ninstall /usr/games/hangman </dev/null
clean:
	/bin/rm -f *.b hangman
