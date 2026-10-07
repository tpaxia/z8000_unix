CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I. -DNROFF
all: nroff
nroff: n1.b n2.b n3.b n4.b n5.b n6.b n7.b n8.b n9.b n10.b ni.b nii.b ntab.b hytab.b suftab.b
	$(CC) -i -s n1.b n2.b n3.b n4.b n5.b n6.b n7.b n8.b n9.b n10.b ni.b nii.b ntab.b hytab.b suftab.b -o nroff
n1.b: /usr/src/cmd/troff/n1.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/troff/n1.c
n2.b: /usr/src/cmd/troff/n2.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/troff/n2.c
n3.b: /usr/src/cmd/troff/n3.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/troff/n3.c
n4.b: /usr/src/cmd/troff/n4.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/troff/n4.c
n5.b: /usr/src/cmd/troff/n5.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/troff/n5.c
n6.b: /usr/src/cmd/troff/n6.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/troff/n6.c
n7.b: /usr/src/cmd/troff/n7.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/troff/n7.c
n8.b: /usr/src/cmd/troff/n8.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/troff/n8.c
n9.b: /usr/src/cmd/troff/n9.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/troff/n9.c
n10.b: /usr/src/cmd/troff/n10.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/troff/n10.c
ni.b: /usr/src/cmd/troff/ni.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/troff/ni.c
nii.b: /usr/src/cmd/troff/nii.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/troff/nii.c
ntab.b: /usr/src/cmd/troff/ntab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/troff/ntab.c
hytab.b: /usr/src/cmd/troff/hytab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/troff/hytab.c
suftab.b: /usr/src/cmd/troff/suftab.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/troff/suftab.c
install: all
	/bin/cp nroff /bin/ninstall
	/bin/mv /bin/ninstall /bin/nroff </dev/null
clean:
	/bin/rm -f *.b nroff
