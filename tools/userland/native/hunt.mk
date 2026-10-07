CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: hunt
hunt: hunt1.b hunt2.b hunt3.b hunt5.b hunt6.b hunt7.b hunt8.b hunt9.b refer3.b glue5.b glue4.b shell.b deliv2.b
	$(CC) -i -s hunt1.b hunt2.b hunt3.b hunt5.b hunt6.b hunt7.b hunt8.b hunt9.b refer3.b glue5.b glue4.b shell.b deliv2.b -o hunt
hunt1.b: /usr/src/cmd/refer/hunt1.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/hunt1.c
hunt2.b: /usr/src/cmd/refer/hunt2.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/hunt2.c
hunt3.b: /usr/src/cmd/refer/hunt3.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/hunt3.c
hunt5.b: /usr/src/cmd/refer/hunt5.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/hunt5.c
hunt6.b: /usr/src/cmd/refer/hunt6.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/hunt6.c
hunt7.b: /usr/src/cmd/refer/hunt7.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/hunt7.c
hunt8.b: /usr/src/cmd/refer/hunt8.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/hunt8.c
hunt9.b: /usr/src/cmd/refer/hunt9.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/hunt9.c
refer3.b: /usr/src/cmd/refer/refer3.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/refer3.c
glue5.b: /usr/src/cmd/refer/glue5.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/glue5.c
glue4.b: /usr/src/cmd/refer/glue4.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/glue4.c
shell.b: /usr/src/cmd/refer/shell.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/shell.c
deliv2.b: /usr/src/cmd/refer/deliv2.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/deliv2.c
install: all
	/bin/cp hunt /usr/lib/refer/ninstall
	/bin/mv /usr/lib/refer/ninstall /usr/lib/refer/hunt </dev/null
clean:
	/bin/rm -f *.b hunt
