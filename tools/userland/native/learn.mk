CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: learn
learn: copy.b dounit.b learn.b list.b mem.b makpipe.b maktee.b mysys.b selsub.b selunit.b start.b whatnow.b wrapup.b
	$(CC) -i -s copy.b dounit.b learn.b list.b mem.b makpipe.b maktee.b mysys.b selsub.b selunit.b start.b whatnow.b wrapup.b -o learn
copy.b: /usr/src/cmd/learn/copy.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/learn/copy.c
dounit.b: /usr/src/cmd/learn/dounit.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/learn/dounit.c
learn.b: /usr/src/cmd/learn/learn.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/learn/learn.c
list.b: /usr/src/cmd/learn/list.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/learn/list.c
mem.b: /usr/src/cmd/learn/mem.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/learn/mem.c
makpipe.b: /usr/src/cmd/learn/makpipe.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/learn/makpipe.c
maktee.b: /usr/src/cmd/learn/maktee.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/learn/maktee.c
mysys.b: /usr/src/cmd/learn/mysys.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/learn/mysys.c
selsub.b: /usr/src/cmd/learn/selsub.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/learn/selsub.c
selunit.b: /usr/src/cmd/learn/selunit.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/learn/selunit.c
start.b: /usr/src/cmd/learn/start.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/learn/start.c
whatnow.b: /usr/src/cmd/learn/whatnow.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/learn/whatnow.c
wrapup.b: /usr/src/cmd/learn/wrapup.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/learn/wrapup.c
install: all
	/bin/cp learn /bin/ninstall
	/bin/mv /bin/ninstall /bin/learn </dev/null
clean:
	/bin/rm -f *.b learn
