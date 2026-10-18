#!/usr/bin/env python3
"""Export a committed tree, then apply QA hooks outside the checkout."""
import argparse
from pathlib import Path
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--repo',type=Path,default=Path(__file__).resolve().parents[5])
p.add_argument('--out',type=Path,required=True)
p.add_argument('--ref',default='HEAD')
a = p.parse_args()
out = a.out.resolve()
out.mkdir() # never overwrite previous experiments
source = out/'src'
source.mkdir()
archive = out/'source.tar'
with archive.open('wb') as f:
    subprocess.run(['git','-C',str(a.repo.resolve()),'archive',a.ref,'games/pikmin1','libs','tools'],stdout=f,check=True)
subprocess.run(['tar','-xf',str(archive),'-C',str(source)],check=True)
with Path(__file__).with_name('test-hooks.patch').open('rb') as f:
    subprocess.run(['patch','--batch','-p1','--no-backup-if-mismatch'],cwd=source,stdin=f,check=True)
print('Exported commit:',subprocess.check_output(['git','-C',str(a.repo.resolve()),'rev-parse',a.ref],text=True).strip())
print('QA source:',source)
print('Configure and build this source separately; never install the hooked executable.')
