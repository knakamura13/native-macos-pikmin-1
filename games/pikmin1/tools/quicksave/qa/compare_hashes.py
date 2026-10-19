#!/usr/bin/env python3
"""Compare all hash columns in a required inclusive tick range; last tick wins."""
import argparse
from pathlib import Path

def read(path):
    rows={}
    for line in path.read_text().splitlines():
        fields=line.split()
        if len(fields)>=9:
            rows[int(fields[0])]=fields[1:]
    return rows

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('left',type=Path)
p.add_argument('right',type=Path)
p.add_argument('--from-tick',type=int,required=True)
p.add_argument('--through-tick',type=int,required=True)
a=p.parse_args()
if a.through_tick<a.from_tick: p.error('tick range must not be empty')
left,right=read(a.left),read(a.right)
for tick in range(a.from_tick,a.through_tick+1):
    if tick not in left or tick not in right:
        raise SystemExit(f'MISSING tick {tick}')
    if left[tick]!=right[tick]:
        raise SystemExit(f'DIVERGED tick {tick}: {left[tick]} != {right[tick]}')
print(f'IDENTICAL: {a.through_tick-a.from_tick+1} ticks ({a.from_tick}..{a.through_tick}), all columns')
