#!/usr/bin/env python3
# Compares two 256x256 RGBA dumps. Exits non-zero if any channel differs by more than
# the tolerance (rounding differences of 1 are expected between implementations).
import sys

W = H = 256
TOLERANCE = 8

a = open(sys.argv[1], 'rb').read()
b = open(sys.argv[2], 'rb').read()
if len(a) != W * H * 4 or len(b) != W * H * 4:
    sys.exit('unexpected dump size')

differing = 0
worst = 0
over_tolerance = 0
for i in range(0, len(a), 4):
    d = max(abs(a[i + k] - b[i + k]) for k in range(4))
    if d:
        differing += 1
    worst = max(worst, d)
    if d > TOLERANCE:
        over_tolerance += 1

print(f'pixels={W * H} differing={differing} worst_channel_diff={worst} over_tolerance={over_tolerance}')
sys.exit(1 if over_tolerance else 0)
