"""Creates header-only synthetic probes. NOT native Blade3D .xmdl models."""
from pathlib import Path
p=Path(__file__).resolve().parent
(p/'blade_xmdl_header_candidate.xmdl').write_bytes(b'xmdl0100' + b'\x00'*8)
(p/'blade_xmdl_bad_header.xmdl').write_bytes(b'not-xmdl-an-actual-model')

# XIR1 fixture from the existing synthetic mesh: overwrite one position float with NaN.
src=(p.parent/'vendor/fragformats_r22/tests/synthetic_mesh.xir')
if src.exists():
    raw=bytearray(src.read_bytes())
    # Read record table instead of assuming a fixed vertex stream offset.
    import struct
    records=struct.unpack_from('<I',raw,4)[0]
    payload=12+records*16
    for i in range(records):
        typ,aux,offset,length=struct.unpack_from('<IIII',raw,12+i*16)
        if typ==3:
            raw[payload+offset+4:payload+offset+8]=struct.pack('<I',0x7fc00000)
            break
    (p/'blade_xir_nan_position.xir').write_bytes(raw)
