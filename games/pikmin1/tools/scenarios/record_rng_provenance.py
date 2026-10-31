#!/usr/bin/env python3
"""Retain exact source/build/input provenance alongside RNG scenario evidence."""
import hashlib
import json
import pathlib
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[4]
build = pathlib.Path(sys.argv[1])
upstream = 'a39f2e11f641b1a717fab0cc44218a8ba5650264'
subprocess.run(['git','-C',str(root),'merge-base','--is-ancestor',upstream,'HEAD'],check=True)
cache = (build/'CMakeCache.txt').read_text()
compiler = next(line.split('=',1)[1] for line in cache.splitlines() if line.startswith('CMAKE_CXX_COMPILER:FILEPATH='))
paths = ['games/pikmin1/pc_port/scenarios/scenario_throw_release.cpp',
         'games/pikmin1/pc_port/netplay/pc_netplay_det.cpp',
         'games/pikmin1/tools/scenarios/run_standin_scenario.cmake']
result = {
    'revision': subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip(),
    'upstream_revision': upstream,
    'harness_base_revision': 'cbf41fca170ad8e5a6323fbc899e38206e30c8f1',
    'rng_order': 'merged upstream #224; conflict arms explicitly resolved to upstream',
    'compiler': subprocess.check_output([compiler,'--version'],text=True),
    'session_seed': 0, 'rates': [30,60],
    'input_source_sha256': {p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in paths},
    'limitations': 'Fork harness and deterministic-float changes remain present; this is not a pure upstream gameplay build.',
}
output = build/'scenario-evidence'
output.mkdir(exist_ok=True)
(output/'metadata.json').write_text(json.dumps(result,indent=2)+'\n')
for filename in ('CMakeCache.txt','CTestTestfile.cmake'):
    (output/filename).write_bytes((build/filename).read_bytes())
