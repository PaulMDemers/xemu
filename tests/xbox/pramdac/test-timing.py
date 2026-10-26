#!/usr/bin/env python3
"""Actual PRAMDAC/reset/load functions with modeled containing device and locks.

Also generate a projection of the actual migration fields for QEMU's VMState
test binary. The projection is NOT a full NV2A snapshot compatibility test.
"""
import json
from pathlib import Path
import re
import subprocess
import sys

root,out=map(Path,sys.argv[1:3]); out.mkdir(parents=True,exist_ok=True)
source=(root/'hw/xbox/nv2a/nv2a.c').read_text()
header=(root/'hw/xbox/nv2a/nv2a_int.h').read_text()
fields=header.split('    struct {\n        uint32_t core_clock_coeff;',1)[1].split('    } pramdac;',1)[0]
stub=Path(__file__).with_name('timing-stub.h').read_text().replace('/* PRAMDAC_FIELDS */',fields)
(out/'nv2a_int.h').write_text(stub)
(out/'regs.h').write_text((root/'hw/xbox/nv2a/nv2a_regs.h').read_text())
(out/'pramdac.c').write_text((root/'hw/xbox/nv2a/pramdac.c').read_text())
functions=[]
for name in ['nv2a_reset','nv2a_pre_load','nv2a_post_load']:
    m=re.search(r'^static (?:int|void) '+name+r'\([^;]+?\n\{.*?^\}',source,re.M|re.S)
    assert m,name
    body=m[0]
    if len(sys.argv)>3 and sys.argv[3]=='drop-'+('reset' if name=='nv2a_reset' else 'preload'):
        body=re.sub(r'    d->pramdac\.fp_(?:vtotal|vsync_start|htotal) = 0;\n','',body)
    functions.append(body)
(out/'reset-load.h').write_text('\n\n'.join(functions))
(out/'host.c').write_text(Path(__file__).with_name('timing-fixture.c').read_text())
cmd=['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie',str(out/'host.c'),'-o',str(out/'host')]
p=subprocess.run(cmd,capture_output=True,text=True)
(out/'compile.log').write_text(p.stdout+p.stderr); assert p.returncode==0,p.stderr
results=[]
for case in ['readback','reset','preload']:
    p=subprocess.run([str(out/'host'),case],capture_output=True,text=True)
    results.append(dict(case=case,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr))
    print(case,p.returncode,p.stdout,p.stderr)
(out/'results.json').write_text(json.dumps(dict(command=cmd,cases=results),indent=2)+'\n')
if 'fp_vtotal' in header:
    # Ensure every old schema entry remains unchanged and in the same order.
    base=subprocess.check_output(['git','show','25e1ee2fd59a451f2ab57e825b4f8a5414220c8b:hw/xbox/nv2a/nv2a.c'],cwd=root,text=True)
    schema=lambda s:s.split('static const VMStateDescription vmstate_nv2a = {',1)[1].split('\n};',1)[0]
    new=schema(source); prior=schema(base)
    added=''.join(f'        VMSTATE_UINT32_V(pramdac.{f}, NV2AState, 6),\n' for f in ['fp_vtotal','fp_vsync_start','fp_htotal'])
    assert new.replace('.version_id = 6,','.version_id = 5,').replace(added,'')==prior
    projected=re.findall(r'        VMSTATE_UINT(?:32|64)(?:_V)?\(pramdac\.[^\n]+\n',new)
    assert len(projected)==7
    migration=Path(__file__).with_name('timing-migration.c').read_text().replace('/* ACTUAL_FIELDS */',''.join(projected))
    preload=re.search(r'static int nv2a_pre_load.*?\n\}',source,re.S)[0]
    zeros=''.join(re.findall(r'    d->pramdac\.fp_[^\n]+\n',preload))
    migration=re.sub(r'    d->pramdac.fp_vtotal = 0;\n    d->pramdac.fp_vsync_start = 0;\n    d->pramdac.fp_htotal = 0;\n',lambda _:zeros,migration)
    (out/'migration-projection.c').write_text(migration)
sys.exit(any(r['returncode'] for r in results))
