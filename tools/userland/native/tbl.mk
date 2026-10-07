CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: tbl
tbl: t0.b t1.b t2.b t3.b t4.b t5.b t6.b t7.b t8.b t9.b tb.b tc.b te.b tf.b tg.b ti.b tm.b ts.b tt.b tu.b tv.b
	$(CC) -i -s t0.b t1.b t2.b t3.b t4.b t5.b t6.b t7.b t8.b t9.b tb.b tc.b te.b tf.b tg.b ti.b tm.b ts.b tt.b tu.b tv.b -o tbl
t0.b: /usr/src/cmd/tbl/t0.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tbl/t0.c
t1.b: /usr/src/cmd/tbl/t1.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tbl/t1.c
t2.b: /usr/src/cmd/tbl/t2.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tbl/t2.c
t3.b: /usr/src/cmd/tbl/t3.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tbl/t3.c
t4.b: /usr/src/cmd/tbl/t4.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tbl/t4.c
t5.b: /usr/src/cmd/tbl/t5.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tbl/t5.c
t6.b: /usr/src/cmd/tbl/t6.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tbl/t6.c
t7.b: /usr/src/cmd/tbl/t7.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tbl/t7.c
t8.b: /usr/src/cmd/tbl/t8.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tbl/t8.c
t9.b: /usr/src/cmd/tbl/t9.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tbl/t9.c
tb.b: /usr/src/cmd/tbl/tb.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tbl/tb.c
tc.b: /usr/src/cmd/tbl/tc.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tbl/tc.c
te.b: /usr/src/cmd/tbl/te.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tbl/te.c
tf.b: /usr/src/cmd/tbl/tf.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tbl/tf.c
tg.b: /usr/src/cmd/tbl/tg.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tbl/tg.c
ti.b: /usr/src/cmd/tbl/ti.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tbl/ti.c
tm.b: /usr/src/cmd/tbl/tm.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tbl/tm.c
ts.b: /usr/src/cmd/tbl/ts.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tbl/ts.c
tt.b: /usr/src/cmd/tbl/tt.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tbl/tt.c
tu.b: /usr/src/cmd/tbl/tu.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tbl/tu.c
tv.b: /usr/src/cmd/tbl/tv.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/tbl/tv.c
install: all
	/bin/cp tbl /bin/ninstall
	/bin/mv /bin/ninstall /bin/tbl </dev/null
clean:
	/bin/rm -f *.b tbl
