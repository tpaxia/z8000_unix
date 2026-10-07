CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: tk
tk: tk.b
	$(CC) -i -s tk.b -o tk
tk.b: /usr/src/cmd/tk.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tk.c
install: all
	/bin/cp tk /bin/ninstall
	/bin/mv /bin/ninstall /bin/tk </dev/null
clean:
	/bin/rm -f *.b tk
