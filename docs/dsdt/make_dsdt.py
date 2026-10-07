#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 URFAVF3MBOY (see NOTICE at the repository root; keep it if you redistribute this file)
"""Rebuild the hotdogb DSDT from the original Project Aloha DSDT.

  python3 make_dsdt.py ORIGINAL_DSDT.aml OUTPUT_DSDT.aml

Every edit is listed below with the bytes it expects to find; the script refuses to run on a different file.
Offsets are into the ORIGINAL file. Edits are applied back to front so earlier offsets stay valid.
"""
import sys

B = bytes.fromhex
# (offset, expected old bytes, new bytes, what it is)
EDITS = [
    # --- Touch (device TSC1, _HID OPOP1003) ---
    (0x57677, B('48'), B('20'),
     'TSC1 _CRS: I2C slave address 0x48 -> 0x20'),
    (0x57651, b'TECC', b'PEP0',
     'TSC1 _DEP: depend on PEP0 instead of TECC (TECC never starts on Windows, so touch never started)'),
    # --- Charging: USB-C sink power list (UCP0 _DSD), 3 entries ---
    (0x54fc7, B('0c2c910100' '0cc8d00200' '0c96c00300'), B('0c22910100' * 3),
     'Sink PDOs: 5V/3.0A, 9V/2.0A, 12V/1.5A  ->  5V/2.9A x3 (0x00019122)'),
    # --- GPU: new SPNL device (insert) ---
    (0x545df, b'', B('5b821453504e4c085f4849440d4d5348573130303400'),
     'Insert Device(SPNL) { Name(_HID, "MSHW1004") } into \\_SB (22 bytes)'),
    # --- GPU: GPU0 _DEP gets \_SB.SPNL (insert) and the lengths that cover it ---
    (0x45cad, b'', B('5c2e5f53425f53504e4c'),
     'GPU0 _DEP: add \\_SB.SPNL (10 bytes)'),
    (0x45c46, B('47060a'), B('41070b'),
     'GPU0 _DEP: package length +10 and element count 10 -> 11'),
    (0x45be8, B('884d'), B('824e'),
     'Device(GPU0) length +10'),
    # --- enclosing \_SB scope length (+32) and table header length (+32) ---
    (0x26, B('04'), B('06'), 'Scope(\\_SB) length +32'),
    (0x04, B('68900500'), B('88900500'), 'DSDT header length 0x59068 -> 0x59088'),
]

def main(src, dst):
    d = bytearray(open(src, 'rb').read())
    assert d[:4] == b'DSDT' and len(d) == 364648, 'not the expected original DSDT'
    for off, old, new, why in sorted(EDITS, key=lambda e: -e[0]):
        assert bytes(d[off:off + len(old)]) == old, 'unexpected bytes at %#x (%s)' % (off, why)
        d[off:off + len(old)] = new
    d[9] = 0
    d[9] = (-sum(d)) % 256          # AML checksum
    assert sum(d) % 256 == 0 and int.from_bytes(d[4:8], 'little') == len(d)
    open(dst, 'wb').write(d)
    print('wrote', dst, len(d), 'bytes')

if __name__ == '__main__':
    main(*sys.argv[1:3])
