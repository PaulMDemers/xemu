#!/usr/bin/env python3
"""Compile the PRAMDAC field projection into QEMU's real VMState tests.

Usage: test-pramdac-vmstate.py SOURCE CONFIGURED_BUILD HOST_TEST_OUTPUT OUTPUT
Run in the same toolchain environment as CONFIGURED_BUILD; then execute the
resulting test-vmstate-pramdac binary on the target OS. This does not launch xemu.
"""
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import sys

root,build,host,out=map(lambda p:Path(p).resolve(),sys.argv[1:5])
out.mkdir(exist_ok=False)
commands=json.loads((build/'compile_commands.json').read_text())
entry=next(e for e in commands if e['file'].endswith('/tests/unit/test-vmstate.c'))
target=entry['output'].split('.p/')[0]
p=subprocess.run(['ninja','-j4',target],cwd=build,capture_output=True,text=True)
(out/'build.log').write_text(p.stdout+p.stderr)
assert p.returncode==0,p.stderr
s=(root/'tests/unit/test-vmstate.c').read_text()
extra=(host/'migration-projection.c').read_text()
assert s.count('int main(int argc, char **argv)')==1
s=s.replace('int main(int argc, char **argv)',extra+'\nint main(int argc, char **argv)')
assert s.count('    g_test_run();')==1
s=s.replace('    g_test_run();','    g_test_add_func("/vmstate/nv2a/pramdac-projection", test_timing_projection);\n    g_test_run();')
generated=out/'test-vmstate-pramdac.c'; generated.write_text(s)
args=shlex.split(entry['command']); obj=out/'test-vmstate-pramdac.o'
for opt in ['-o','-MQ','-MF']:
    if opt in args: args[args.index(opt)+1]=str(obj)+('.d' if opt=='-MF' else '')
args[args.index('-c')+1]=str(generated)
p=subprocess.run(args,cwd=build,capture_output=True,text=True)
(out/'compile.log').write_text(p.stdout+p.stderr); assert p.returncode==0,p.stderr
lines=subprocess.check_output(['ninja','-t','commands',target],cwd=build,text=True).splitlines()
link=shlex.split(lines[-1]); assert entry['output'] in link
link=[str(obj) if a==entry['output'] else a for a in link]
exe=out/('test-vmstate-pramdac.exe' if target.endswith('.exe') else 'test-vmstate-pramdac')
link[link.index('-o')+1]=str(exe)
p=subprocess.run(link,cwd=build,capture_output=True,text=True)
(out/'link.log').write_text(p.stdout+p.stderr); assert p.returncode==0,p.stderr
(out/'commands.json').write_text(json.dumps(dict(compile=args,link=link,sha256=hashlib.sha256(exe.read_bytes()).hexdigest()),indent=2)+'\n')
print(exe)
