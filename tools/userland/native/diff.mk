CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: diff
diff: diff.b
	$(CC) -i -s diff.b -o diff
diff.b: /usr/src/cmd/diff.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/diff.c
install: all
	/bin/cp diff /bin/ninstall
	/bin/mv /bin/ninstall /bin/diff </dev/null
clean:
	/bin/rm -f *.b diff
