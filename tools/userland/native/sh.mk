CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: sh
sh: msg.b args.b print.b io.b ctype.b xec.b string.b word.b cmd.b main.b error.b blok.b service.b builtin.b name.b setbrk.b stak.b fault.b expand.b macro.b
	$(CC) -i -s msg.b args.b print.b io.b ctype.b xec.b string.b word.b cmd.b main.b error.b blok.b service.b builtin.b name.b setbrk.b stak.b fault.b expand.b macro.b -o sh
msg.b: /usr/src/cmd/sh/msg.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/sh/msg.c
args.b: /usr/src/cmd/sh/args.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/sh/args.c
print.b: /usr/src/cmd/sh/print.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/sh/print.c
io.b: /usr/src/cmd/sh/io.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/sh/io.c
ctype.b: /usr/src/cmd/sh/ctype.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/sh/ctype.c
xec.b: /usr/src/cmd/sh/xec.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/sh/xec.c
string.b: /usr/src/cmd/sh/string.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/sh/string.c
word.b: /usr/src/cmd/sh/word.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/sh/word.c
cmd.b: /usr/src/cmd/sh/cmd.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/sh/cmd.c
main.b: /usr/src/cmd/sh/main.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/sh/main.c
error.b: /usr/src/cmd/sh/error.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/sh/error.c
blok.b: /usr/src/cmd/sh/blok.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/sh/blok.c
service.b: /usr/src/cmd/sh/service.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/sh/service.c
builtin.b: /usr/src/cmd/sh/builtin.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/sh/builtin.c
name.b: /usr/src/cmd/sh/name.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/sh/name.c
setbrk.b: /usr/src/cmd/sh/setbrk.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/sh/setbrk.c
stak.b: /usr/src/cmd/sh/stak.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/sh/stak.c
fault.b: /usr/src/cmd/sh/fault.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/sh/fault.c
expand.b: /usr/src/cmd/sh/expand.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/sh/expand.c
macro.b: /usr/src/cmd/sh/macro.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/sh/macro.c
install: all
	/bin/cp sh /bin/ninstall
	/bin/rm -f /bin/sh.$$$$ && /bin/ln /bin/sh /bin/sh.$$$$ && /bin/mv /bin/ninstall /bin/sh </dev/null
clean:
	/bin/rm -f *.b sh
