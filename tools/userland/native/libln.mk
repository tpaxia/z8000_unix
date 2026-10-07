CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002
all: libln.a
libln.a: main.b allprint.b reject.b yyless.b yywrap.b
	/bin/rm -f libln.a
	/bin/ar qc libln.a main.b allprint.b reject.b yyless.b yywrap.b
main.b: /usr/src/cmd/lex/lib/main.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/lex/lib/main.c
allprint.b: /usr/src/cmd/lex/lib/allprint.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/lex/lib/allprint.c
reject.b: /usr/src/cmd/lex/lib/reject.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/lex/lib/reject.c
yyless.b: /usr/src/cmd/lex/lib/yyless.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/lex/lib/yyless.c
yywrap.b: /usr/src/cmd/lex/lib/yywrap.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/lex/lib/yywrap.c
install: all
	/bin/cp libln.a /lib/libln.a
clean:
	/bin/rm -f *.b libln.a
