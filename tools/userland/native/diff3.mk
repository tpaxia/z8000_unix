CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: diff3
diff3: diff3.b
	$(CC) -i -s diff3.b -o diff3
diff3.b: /usr/src/cmd/diff3.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/diff3.c
install: all
	/bin/cp diff3 /usr/lib/ninstall
	/bin/mv /usr/lib/ninstall /usr/lib/diff3 </dev/null
clean:
	/bin/rm -f *.b diff3
