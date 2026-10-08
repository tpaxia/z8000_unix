CC=/bin/cc
CFLAGS=-O -I.
all: asz8k
asz8k: aout.b aparse.b aoutput.b asz8kglo.b aascom.b axref.b ainput.b asz8kgrm.b amisc.b atoken.b aglocom.b asz8k.b avm.b alook.b adirec.b ainit.b
	$(CC) -i -s aout.b aparse.b aoutput.b asz8kglo.b aascom.b axref.b ainput.b asz8kgrm.b amisc.b atoken.b aglocom.b asz8k.b avm.b alook.b adirec.b ainit.b -o asz8k
aparse.b: aparse.c acom.h asz8k.h obj.h
	$(CC) $(CFLAGS) -c aparse.c
aoutput.b: aoutput.c acom.h asz8k.h obj.h
	$(CC) $(CFLAGS) -c aoutput.c
asz8kglo.b: asz8kglo.c acom.h asz8k.h obj.h
	$(CC) $(CFLAGS) -c asz8kglo.c
aascom.b: aascom.c acom.h asz8k.h obj.h
	$(CC) $(CFLAGS) -c aascom.c
axref.b: axref.c acom.h asz8k.h obj.h
	$(CC) $(CFLAGS) -c axref.c
ainput.b: ainput.c acom.h asz8k.h obj.h
	$(CC) $(CFLAGS) -c ainput.c
asz8kgrm.b: asz8kgrm.c acom.h asz8k.h obj.h
	$(CC) $(CFLAGS) -c asz8kgrm.c
amisc.b: amisc.c acom.h asz8k.h obj.h
	$(CC) $(CFLAGS) -c amisc.c
atoken.b: atoken.c acom.h asz8k.h obj.h
	$(CC) $(CFLAGS) -c atoken.c
aglocom.b: aglocom.c acom.h asz8k.h obj.h
	$(CC) $(CFLAGS) -c aglocom.c
asz8k.b: asz8k.c acom.h asz8k.h obj.h
	$(CC) $(CFLAGS) -c asz8k.c
avm.b: avm.c acom.h asz8k.h obj.h
	$(CC) $(CFLAGS) -c avm.c
alook.b: alook.c acom.h asz8k.h obj.h
	$(CC) $(CFLAGS) -c alook.c
adirec.b: adirec.c acom.h asz8k.h obj.h
	$(CC) $(CFLAGS) -c adirec.c
ainit.b: ainit.c acom.h asz8k.h obj.h
	$(CC) $(CFLAGS) -c ainit.c
aout.b: aout.c acom.h obj.h
	$(CC) $(CFLAGS) -c aout.c
