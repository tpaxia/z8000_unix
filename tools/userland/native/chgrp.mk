CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: chgrp
chgrp: chgrp.b
	$(CC) -i -s chgrp.b -o chgrp
chgrp.b: /usr/src/cmd/chgrp.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/chgrp.c
install: all
	/bin/cp chgrp /bin/ninstall
	/bin/mv /bin/ninstall /bin/chgrp </dev/null
clean:
	/bin/rm -f *.b chgrp
