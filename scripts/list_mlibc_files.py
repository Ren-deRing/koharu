import os
import sys

base = os.path.realpath(sys.argv[1])

out = []
for entry in sys.argv[2:]:
    start = os.path.join(base, entry)
    if os.path.isfile(start):
        out.append(os.path.relpath(start, base))
        continue
    for dirpath, dirnames, filenames in os.walk(start):
        dirnames[:] = [
            d for d in dirnames
            if d not in ('.git', '.cache', 'build')
        ]
        for f in filenames:
            out.append(os.path.relpath(os.path.join(dirpath, f), base))

for p in sorted(out):
    print(p)
