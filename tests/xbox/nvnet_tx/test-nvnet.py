#!/usr/bin/env python3
"""Isolated actual-source NVNET tests; DMA, network and PCI IRQ are modeled.

Usage: python3 test-nvnet.py SOURCE_ROOT OUTPUT_DIR [CASE]
No full-device, hardware, QEMU dispatch or network-backend claim is made.
"""
import json
import os
from pathlib import Path
import re
import subprocess
import sys

root, out = map(Path, sys.argv[1:3])
mode = 'tx'
out.mkdir(parents=True, exist_ok=True)
source = (root/'hw/xbox/mcpx/nvnet/nvnet.c').read_text()
names = ['get_reg_ext','get_reg','set_reg_ext','set_reg','or_reg','and_reg',
         'set_reg_with_mask','update_irq','set_intr_status','get_tx_ring_size',
         'reset_descriptor_ring_pointers','link_up','dma_enabled','set_dma_idle',
         'load_ring_desc','store_ring_desc','tx_enabled','can_transmit',
         'update_current_tx_ring_desc_addr','advance_next_tx_ring_desc_addr',
         'dma_packet_from_guest']
if mode == 'mmio':
    names += ['nvnet_mmio_read', 'nvnet_mmio_write']
functions = []
for name in names:
    match = re.search(r'^static [^\n]+\b'+name+r'\([^;]+?\n\{.*?^\}', source, re.M | re.S)
    assert match, name
    functions.append(match[0].replace('static ', 'static __attribute__((unused)) ',1))
fixture = Path(__file__).with_name('nvnet-fixture.c').read_text()
generated = fixture.replace('/* ACTUAL_FUNCTIONS */', '\n\n'.join(functions))
(out/'nvnet-test.c').write_text(generated)
command = [os.environ.get('CC','gcc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra',
           '-Werror', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
           '-fno-pie','-no-pie', '-DTEST_'+mode.upper(), '-I'+str(root), '-I'+str(root/'include'),
           str(out/'nvnet-test.c'), '-o', str(out/'nvnet-test')]
p = subprocess.run(command, capture_output=True, text=True)
(out/'compile.log').write_text(p.stdout+p.stderr)
assert p.returncode == 0, p.stderr
cases = (['batch','wrap','full','split','invalid','disabled','flags','max-ring',
          'buffer-exact','oversized','next-outside','length-ffff'] if mode=='tx' else
         ['mac-widths','w1c','mask','control','mdio','nonmac-lane','unaligned-mac'])
if len(sys.argv)>3:
    cases = [sys.argv[3]]
results = []
for case in cases:
    p = subprocess.run([str(out/'nvnet-test'),case], capture_output=True, text=True)
    # Assertions remain enabled. These unsupported paths must still be rejected.
    expected_abort = case in ['oversized','nonmac-lane','unaligned-mac']
    passed = p.returncode == (-6 if expected_abort else 0)
    results.append(dict(case=case, passed=passed, returncode=p.returncode,
                        expected='SIGABRT' if expected_abort else 'success',
                        stdout=p.stdout, stderr=p.stderr))
(out/'results.json').write_text(json.dumps(dict(command=command, cases=results),indent=2)+'\n')
for r in results:
    print(('PASS' if r['passed'] else 'FAIL')+': '+r['case']+' ('+str(r['returncode'])+')')
sys.exit(not all(r['passed'] for r in results))
