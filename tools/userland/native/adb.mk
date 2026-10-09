CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I.
all: adb
adb: access.b command.b expr.b findfn.b format.b input.b opset.b main.b message.b output.b pcs.b print.b runpcs.b setup.b sym.b
	$(CC) -i -s access.b command.b expr.b findfn.b format.b input.b opset.b main.b message.b output.b pcs.b print.b runpcs.b setup.b sym.b -o adb
access.b: /usr/src/cmd/adb/access.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/adb/access.c
command.b: /usr/src/cmd/adb/command.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/adb/command.c
expr.b: /usr/src/cmd/adb/expr.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/adb/expr.c
findfn.b: /usr/src/cmd/adb/findfn.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/adb/findfn.c
format.b: /usr/src/cmd/adb/format.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/adb/format.c
input.b: /usr/src/cmd/adb/input.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/adb/input.c
opset.b: /usr/src/cmd/adb/opset.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/adb/opset.c
main.b: /usr/src/cmd/adb/main.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/adb/main.c
message.b: /usr/src/cmd/adb/message.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/adb/message.c
output.b: /usr/src/cmd/adb/output.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/adb/output.c
pcs.b: /usr/src/cmd/adb/pcs.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/adb/pcs.c
print.b: /usr/src/cmd/adb/print.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/adb/print.c
runpcs.b: /usr/src/cmd/adb/runpcs.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/adb/runpcs.c
setup.b: /usr/src/cmd/adb/setup.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/adb/setup.c
sym.b: /usr/src/cmd/adb/sym.c
	$(CC) $(CFLAGS) -c /usr/src/cmd/adb/sym.c
install: all
	/bin/cp adb /bin/adb
clean:
	/bin/rm -f *.b adb
