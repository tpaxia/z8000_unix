# Object utilities

Shared host/native `nm`, `size` and `strip` for the port's a.out and s.out
formats, plus a V7 `nlist` ABI adapter. Original V7 source files remain intact.

Build host tools with `make -C tools/sout-utils`. After the
[native s.out C trial](../../doc/development/native-rebuild.md#sout-native-c-pipeline),
run `python3 tools/sout-utils/test.py` to compile and test these sources inside
V7. Results are under `tests/build/sout-utils`.

See the [object utility reference](../../doc/toolchain/object-utilities.md)
for behavior and limits.
