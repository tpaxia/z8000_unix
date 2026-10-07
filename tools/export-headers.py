#!/usr/bin/env python3
"""Export V7 kernel ABI headers; --check fails if installed copies are stale."""
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[1]
kernel = root / 'v7z8000/usr/sys/h'
public = root / 'v7z8000/usr/include/sys'
check = '--check' in sys.argv
stale = []
param = (kernel / 'param.h').read_text()
types = param[param.index('typedef\tstruct'):]
types += '\n' + param[param.index('/* major part of a device */'):param.index('typedef\tstruct')]
types = '#ifndef _SYS_TYPES_H\n#define _SYS_TYPES_H\n' + types + '\n#endif\n'
dst = public / 'types.h'
if not dst.exists() or dst.read_text() != types:
    stale.append(str(dst.relative_to(root)))
    if not check:
        dst.write_text(types)
for src in sorted(kernel.glob('*.h')):
    dst = public / src.name
    text = src.read_text()
    if src.name == 'param.h':
        # Public types are independently includable; do not duplicate typedefs.
        text = text.split('/* major part of a device */')[0]
        text += '#include <sys/types.h>\n'
        text = '#ifndef _SYS_PARAM_H\n#define _SYS_PARAM_H\n' + text + '\n#endif\n'
    elif src.name == 'user.h':
        text = text.replace('#define u (*(struct user *)0xF000)', 'extern struct user u;')
        text = '#ifndef _SYS_USER_H\n#define _SYS_USER_H\n' + text + '\n#endif\n'
    if not dst.exists() or dst.read_text() != text:
        stale.append(str(dst.relative_to(root)))
        if not check:
            dst.write_text(text)
if check and stale:
    raise SystemExit('Stale public headers; run python3 tools/export-headers.py:\n' + '\n'.join(stale))
print('Public kernel headers: ' + ('checked' if check else f'{len(stale)} updated'))
