CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: structure
structure: 0.alloc.b 0.args.b 0.def.b 0.extr.b 0.graph.b 0.list.b 0.parts.b 0.string.b 1.finish.b 1.form.b 1.fort.b 1.hash.b 1.init.b 1.line.b 1.main.b 1.node.b 1.recog.b 1.tables.b 2.dfs.b 2.dom.b 2.head.b 2.inarc.b 2.main.b 2.tree.b 3.branch.b 3.flow.b 3.loop.b 3.main.b 3.reach.b 3.then.b 4.brace.b 4.form.b 4.main.b 4.out.b main.b
	$(CC) -i -s 0.alloc.b 0.args.b 0.def.b 0.extr.b 0.graph.b 0.list.b 0.parts.b 0.string.b 1.finish.b 1.form.b 1.fort.b 1.hash.b 1.init.b 1.line.b 1.main.b 1.node.b 1.recog.b 1.tables.b 2.dfs.b 2.dom.b 2.head.b 2.inarc.b 2.main.b 2.tree.b 3.branch.b 3.flow.b 3.loop.b 3.main.b 3.reach.b 3.then.b 4.brace.b 4.form.b 4.main.b 4.out.b main.b -o structure
0.alloc.b: /usr/src/cmd/struct/0.alloc.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/0.alloc.c
0.args.b: /usr/src/cmd/struct/0.args.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/0.args.c
0.def.b: /usr/src/cmd/struct/0.def.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/0.def.c
0.extr.b: /usr/src/cmd/struct/0.extr.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/0.extr.c
0.graph.b: /usr/src/cmd/struct/0.graph.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/0.graph.c
0.list.b: /usr/src/cmd/struct/0.list.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/0.list.c
0.parts.b: /usr/src/cmd/struct/0.parts.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/0.parts.c
0.string.b: /usr/src/cmd/struct/0.string.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/0.string.c
1.finish.b: /usr/src/cmd/struct/1.finish.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/1.finish.c
1.form.b: /usr/src/cmd/struct/1.form.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/1.form.c
1.fort.b: /usr/src/cmd/struct/1.fort.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/1.fort.c
1.hash.b: /usr/src/cmd/struct/1.hash.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/1.hash.c
1.init.b: /usr/src/cmd/struct/1.init.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/1.init.c
1.line.b: /usr/src/cmd/struct/1.line.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/1.line.c
1.main.b: /usr/src/cmd/struct/1.main.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/1.main.c
1.node.b: /usr/src/cmd/struct/1.node.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/1.node.c
1.recog.b: /usr/src/cmd/struct/1.recog.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/1.recog.c
1.tables.b: /usr/src/cmd/struct/1.tables.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/1.tables.c
2.dfs.b: /usr/src/cmd/struct/2.dfs.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/2.dfs.c
2.dom.b: /usr/src/cmd/struct/2.dom.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/2.dom.c
2.head.b: /usr/src/cmd/struct/2.head.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/2.head.c
2.inarc.b: /usr/src/cmd/struct/2.inarc.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/2.inarc.c
2.main.b: /usr/src/cmd/struct/2.main.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/2.main.c
2.tree.b: /usr/src/cmd/struct/2.tree.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/2.tree.c
3.branch.b: /usr/src/cmd/struct/3.branch.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/3.branch.c
3.flow.b: /usr/src/cmd/struct/3.flow.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/3.flow.c
3.loop.b: /usr/src/cmd/struct/3.loop.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/3.loop.c
3.main.b: /usr/src/cmd/struct/3.main.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/3.main.c
3.reach.b: /usr/src/cmd/struct/3.reach.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/3.reach.c
3.then.b: /usr/src/cmd/struct/3.then.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/3.then.c
4.brace.b: /usr/src/cmd/struct/4.brace.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/4.brace.c
4.form.b: /usr/src/cmd/struct/4.form.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/4.form.c
4.main.b: /usr/src/cmd/struct/4.main.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/4.main.c
4.out.b: /usr/src/cmd/struct/4.out.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/4.out.c
main.b: /usr/src/cmd/struct/main.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/struct/main.c
install: all
	/bin/cp structure /usr/lib/struct/ninstall
	/bin/mv /usr/lib/struct/ninstall /usr/lib/struct/structure </dev/null
clean:
	/bin/rm -f *.b structure
