CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: refer
refer: glue1.b glue2.b glue3.b glue4.b glue5.b refer0.b refer1.b refer2.b refer4.b refer5.b refer6.b refer7.b refer8.b hunt2.b hunt3.b hunt5.b hunt6.b hunt7.b hunt8.b hunt9.b mkey3.b shell.b deliv2.b
	$(CC) -i -s glue1.b glue2.b glue3.b glue4.b glue5.b refer0.b refer1.b refer2.b refer4.b refer5.b refer6.b refer7.b refer8.b hunt2.b hunt3.b hunt5.b hunt6.b hunt7.b hunt8.b hunt9.b mkey3.b shell.b deliv2.b -o refer
glue1.b: /usr/src/cmd/refer/glue1.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/glue1.c
glue2.b: /usr/src/cmd/refer/glue2.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/glue2.c
glue3.b: /usr/src/cmd/refer/glue3.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/glue3.c
glue4.b: /usr/src/cmd/refer/glue4.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/glue4.c
glue5.b: /usr/src/cmd/refer/glue5.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/glue5.c
refer0.b: /usr/src/cmd/refer/refer0.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/refer0.c
refer1.b: /usr/src/cmd/refer/refer1.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/refer1.c
refer2.b: /usr/src/cmd/refer/refer2.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/refer2.c
refer4.b: /usr/src/cmd/refer/refer4.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/refer4.c
refer5.b: /usr/src/cmd/refer/refer5.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/refer5.c
refer6.b: /usr/src/cmd/refer/refer6.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/refer6.c
refer7.b: /usr/src/cmd/refer/refer7.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/refer7.c
refer8.b: /usr/src/cmd/refer/refer8.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/refer8.c
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
mkey3.b: /usr/src/cmd/refer/mkey3.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/mkey3.c
shell.b: /usr/src/cmd/refer/shell.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/shell.c
deliv2.b: /usr/src/cmd/refer/deliv2.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/refer/deliv2.c
install: all
	/bin/cp refer /bin/ninstall
	/bin/mv /bin/ninstall /bin/refer </dev/null
clean:
	/bin/rm -f *.b refer
