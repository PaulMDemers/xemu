#!/usr/bin/env python3
"""Execute actual callback/worker policy with modeled renderer and scheduling."""
import json
from pathlib import Path
import re
import subprocess
import sys

root, out = map(Path, sys.argv[1:3])
out.mkdir(parents=True, exist_ok=True)
s = (root/'hw/xbox/nv2a/pgraph/gl/surface.c').read_text()
functions = []
for name in ['check_surface_overlaps_range','surface_access_callback','pgraph_gl_process_pending_downloads']:
    m = re.search(r'^(?:static )?(?:bool|void) '+name+r'\([^;]+?\n\{.*?^\}',s,re.M|re.S)
    assert m, name
    body=m[0]
    if len(sys.argv)>3 and sys.argv[3]=='drop-request-guard':
        body=body.replace(' &&\n                d->pgraph.primitive_mode == PRIM_TYPE_INVALID','')
    if len(sys.argv)>3 and sys.argv[3]=='drop-worker-guard':
        body=body.replace('if (pg->primitive_mode != PRIM_TYPE_INVALID)', 'if (false)')
    functions.append(body)
fixture = Path(__file__).with_name('surface-fixture.c').read_text()
(out/'surface-test.c').write_text(fixture.replace('/* ACTUAL_FUNCTIONS */','\n\n'.join(functions)))
cmd = ['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie',str(out/'surface-test.c'),'-o',str(out/'surface-test')]
p = subprocess.run(cmd,capture_output=True,text=True)
(out/'compile.log').write_text(p.stdout+p.stderr)
assert p.returncode==0,p.stderr
p = subprocess.run([str(out/'surface-test')],capture_output=True,text=True)
(out/'results.json').write_text(json.dumps(dict(command=cmd,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr,scope='Actual callback/worker; modeled locks, scheduler, download and invalidation. No GL, guest, reset, swizzle or hardware qualification.'),indent=2)+'\n')
print(p.stdout,p.stderr)
sys.exit(p.returncode)
