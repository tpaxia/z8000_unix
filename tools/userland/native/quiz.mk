CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: quiz
quiz: quiz.b
	$(CC) -i -s quiz.b -o quiz
quiz.b: /usr/src/games/quiz.c
	$(CC) $(CFLAGS) -c /usr/src/games/quiz.c
install: all
	/bin/cp quiz /usr/games/ninstall
	/bin/mv /usr/games/ninstall /usr/games/quiz </dev/null
clean:
	/bin/rm -f *.b quiz
