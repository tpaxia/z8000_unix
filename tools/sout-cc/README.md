# Native s.out C pipeline trial

`build.py` prepares startup/libc and minimum cross-built tool seeds.
`test.py` boots V7 and rebuilds libc, asz8k, ldz8 and cc with native tools.
The [native rebuild procedure](../../doc/development/native-rebuild.md#sout-native-c-pipeline)
describes prerequisites, commands, artifacts and validation. Compiler behavior
is documented in [native development](../../doc/toolchain/native-development.md),
assembler syntax in [asz8k](../../doc/toolchain/asz8k.md), and object linking in
[ldz8](../../doc/toolchain/ldz8.md).
