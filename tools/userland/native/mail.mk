CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: mail
mail: mail.b
	$(CC) -i -s mail.b -o mail
mail.b: /usr/src/cmd/mail.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/mail.c
install: all
	/bin/cp mail /bin/ninstall
	/bin/mv /bin/ninstall /bin/mail </dev/null
clean:
	/bin/rm -f *.b mail
