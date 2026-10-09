#!/usr/bin/env python3
"""Build compiler passes through the common s.out bootstrap."""
from pathlib import Path
import importlib.util
import sys
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/native-cc'))
spec=importlib.util.spec_from_file_location('native_seed',ROOT/'tools/native-cc/build.py')
seed=importlib.util.module_from_spec(spec);spec.loader.exec_module(seed)
if sys.argv[1:] not in ([],['--no-compact']):
    raise SystemExit('usage: build.py [--no-compact]')
seed.build(no_compact=bool(sys.argv[1:]))
