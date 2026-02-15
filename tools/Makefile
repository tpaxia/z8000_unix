CC = cc
CFLAGS = -Wall -O2

# ACK toolchain
ACK_ROOT = ../ack/.obj/staging
ACK = ACKDIR=$(ACK_ROOT) $(ACK_ROOT)/bin/ack
ACK_LED = $(ACK_ROOT)/lib/ack/em_led

v7mkfs: v7mkfs.c
	$(CC) $(CFLAGS) -o $@ $<

# Build init binary (V7 a.out format)
init.o: init.s
	$(ACK) -mz8000 -z8002 -c -o $@ $<

init.out: init.o
	$(ACK_LED) -b0:0 $< -o $@

init: init.out ack2v7.py
	python3 ack2v7.py $< $@

root.img: v7mkfs proto.small init
	./v7mkfs root.img proto.small

clean:
	rm -f v7mkfs root.img init init.o init.out

.PHONY: clean
