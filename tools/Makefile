CC = cc
CFLAGS = -Wall -O2

v7mkfs: v7mkfs.c
	$(CC) $(CFLAGS) -o $@ $<

root.img: v7mkfs proto.small
	./v7mkfs root.img proto.small

clean:
	rm -f v7mkfs root.img

.PHONY: clean
