# Object utilities

Shared s.out reader for host/native `nm`, `size`, `strip`, make, prof and
libc `nlist`. Command and libc implementations live in `v7z8000/usr/src`;
the pristine PDP-11 sources remain in `v7unix`. There are no legacy readers.

Build host tools with `make -C tools/sout-utils`. After the
[native s.out C trial](../../doc/development/native-rebuild.md#sout-native-c-pipeline),
run `python3 tools/sout-utils/test.py` to compile and test these sources inside
V7. Results are under `tests/build/sout-utils`.

See the [object utility reference](../../doc/toolchain/object-utilities.md)
for behavior and limits.
