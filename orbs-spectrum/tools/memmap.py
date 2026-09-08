#!/usr/bin/env python3
"""Print where the areas landed and how much room is left, from the .map."""
import re, sys

areas = {}
cur = None
for line in open(sys.argv[1]):
    m = re.match(r"^(\w+)\s+([0-9A-F]{8})\s+([0-9A-F]{8})\s*=\s*(\d+)\.", line)
    if m:
        areas[m.group(1)] = (int(m.group(2), 16), int(m.group(3), 16))
code_end = max((a + s for n, (a, s) in areas.items()
                if n in ("_CODE", "_INITIALIZER", "_GSINIT", "_GSFINAL", "_HOME")), default=0)
data_end = max((a + s for n, (a, s) in areas.items()
                if n in ("_DATA", "_INITIALIZED", "_BSS")), default=0)
for n, (a, s) in sorted(areas.items(), key=lambda kv: kv[1][0]):
    if s:
        print(f"  {n:14s} 0x{a:04X}-0x{a + s - 1:04X}  {s:6d} bytes")
print(f"  code ends 0x{code_end:04X}, {0xFA00 - code_end} bytes free below the stack")
print(f"  data ends 0x{data_end:04X}, {0x8000 - data_end} bytes free below the code")
if code_end > 0xFA00:
    sys.exit("CODE OVERFLOWS INTO THE STACK")
if data_end > 0x8000:
    sys.exit("DATA OVERFLOWS INTO THE CODE")
