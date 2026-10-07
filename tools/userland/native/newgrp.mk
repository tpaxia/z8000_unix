CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: newgrp
newgrp: newgrp.b
	$(CC) -i -s newgrp.b -o newgrp
newgrp.b: /usr/src/cmd/newgrp.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/newgrp.c
install: all
	/bin/cp newgrp /bin/ninstall
	/bin/mv /bin/ninstall /bin/newgrp </dev/null
clean:
	/bin/rm -f *.b newgrp
