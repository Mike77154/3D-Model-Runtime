#!/bin/sh
set -eu
mkdir -p tests/generated_r22
python3 - <<'PY'
import struct
from pathlib import Path
p=Path('tests/generated_r22')
def ugh(name,rs,props=b'',header_start=16,version=0x10001):
 h=struct.pack('<4sIII',b'ugh0',version,header_start,len(rs))
 p.joinpath(name).write_bytes(h+b''.join(struct.pack('<7I',*r) for r in rs)+props)
# Candidate contiguous 28-byte records, not native UGH. One-based references.
# attributes for node 0 point to 16+28*3 and include a single 8-byte candidate property.
ugh('onebased.ugh',[(0,1,100,0,2,0,0),(0,0,0,0,3,0,0),(0,0,0,0,0,0,0)],struct.pack('<II',17,99))
ugh('zero_based.ugh',[(0,0,0,0,1,0xffffffff,0),(0,0,0,0,2,0xffffffff,0),(0,0,0,0,0xffffffff,0xffffffff,0)])
ugh('cycle.ugh',[(0,0,0,0,1,0xffffffff,0),(0,0,0,0,0,0xffffffff,0)])
ugh('outofrange.ugh',[(0,1,0xfffffffe,0,1000,0xffffffff,0),(0,0,0,0,0xffffffff,0xffffffff,0)])
# Two-stage stream-shaped fixture matching the *sequence* observed in the
# UGH importer, NOT a native serialized fragMOTION character.
p.joinpath('two_stage.ugh').write_bytes(struct.pack('<4sIII',b'ugh0',32,0,0) + bytes(16) + struct.pack('<4sIII',b'TEST',0x10001,48,1) + struct.pack('<7I',76,0,0,0,0xffffffff,0xffffffff,0) + bytes(8))
p.joinpath('bad_inner.ugh').write_bytes(struct.pack('<4sIII',b'ugh0',100000,0,0)+bytes(50))
p.joinpath('new_version.ugh').write_bytes(struct.pack('<4sIII',b'ugh0',32,0,0) + bytes(16) + struct.pack('<4sIII',b'TEST',0x10002,48,1)+bytes(28))
# Native-layout hypothesis: root at 48, its firstChild at 76, child nextSibling at 104.
# Each record points to a block holding 3 dwords (type/packed-id/flags).
root=struct.pack('<7I',132,1,168,176,76,0xffffffff,0)
child=struct.pack('<7I',144,0,0,184,0xffffffff,104,0)
sib=struct.pack('<7I',156,0,0,248,0xffffffff,0xffffffff,0)
head=struct.pack('<4sIII',b'ugh0',32,0,0)+bytes(16)+struct.pack('<4sIII',b'TEST',0x10001,48,3)
objects=root+child+sib
payload=struct.pack('<III',0x2e,0x1001,1)+struct.pack('<III',0x38,0x1002,1)+struct.pack('<III',0x3e,0x1003,1)+struct.pack('<II',19,77)+struct.pack('<II',4,2)+struct.pack('<16I',*range(16))+struct.pack('<III',3,0xffffffff,0)
p.joinpath('linked.ugh').write_bytes(head+objects+payload)
cycle=bytearray(head+objects+payload)
struct.pack_into('<I',cycle,104+20,76) # sibling points back to child
p.joinpath('linked_cycle.ugh').write_bytes(cycle)
invalid_child=bytearray(head+objects+payload)
struct.pack_into('<I',invalid_child,48+16,0xffffff00)
p.joinpath('linked_bad_edge.ugh').write_bytes(invalid_child)
p.joinpath('notugh.ugh').write_bytes(b'xxxx'+bytes(40))
p.joinpath('truncated.ugh').write_bytes(struct.pack('<4sIII',b'ugh0',0x10001,16,400)+bytes(5))
PY
./ughgraph tests/generated_r22/onebased.ugh tests/generated_r22/onebased.tsv tests/generated_r22/onebased.dot
grep -q 'one_based' tests/generated_r22/onebased.tsv
grep -q 'property_candidate' tests/generated_r22/onebased.tsv
grep -q 'firstChild' tests/generated_r22/onebased.dot
./ughgraph tests/generated_r22/zero_based.ugh tests/generated_r22/zero_based.tsv tests/generated_r22/zero_based.dot
grep -q 'zero_based' tests/generated_r22/zero_based.tsv
./ughgraph tests/generated_r22/cycle.ugh tests/generated_r22/cycle.tsv
awk -F '\t' '$1=="indexing_candidate" && $3=="cycles" && $4>0 {seen=1} END{exit !seen}' tests/generated_r22/cycle.tsv
./ughgraph tests/generated_r22/outofrange.ugh tests/generated_r22/outofrange.tsv
grep -q 'invalid_or_unverified_arrays' tests/generated_r22/outofrange.tsv
./ughgraph tests/generated_r22/truncated.ugh tests/generated_r22/truncated.tsv
grep -q 'NO_BOUNDED_TABLE_AT_DATASTART' tests/generated_r22/truncated.tsv
if ./ughgraph tests/generated_r22/notugh.ugh >/dev/null 2>&1;then echo 'Invalid UGH accepted' >&2;exit 1;fi
./ughnativeprobe tests/generated_r22/two_stage.ugh tests/generated_r22/two_stage.tsv
grep -q 'TWO_STAGE_LAYOUT_FITS_LOADER_READ_SEQUENCE' tests/generated_r22/two_stage.tsv
./ughnativeprobe tests/generated_r22/bad_inner.ugh tests/generated_r22/bad_inner.tsv
grep -q 'NO_INNER_HEADER' tests/generated_r22/bad_inner.tsv
./ughnativeprobe tests/generated_r22/new_version.ugh tests/generated_r22/new_version.tsv
grep -q 'VERSION_OUTSIDE_SUPPORTED' tests/generated_r22/new_version.tsv
if ./ughnativeprobe tests/generated_r22/notugh.ugh >/dev/null 2>&1;then exit 1;fi
./ughtree tests/generated_r22/linked.ugh tests/generated_r22/linked.tsv tests/generated_r22/linked.dot
grep -q '^summary.*objects_walked.*3' tests/generated_r22/linked.tsv
grep -q 'property' tests/generated_r22/linked.tsv
grep -q 'mesh_payload' tests/generated_r22/linked.tsv
grep -q 'matrix_raw' tests/generated_r22/linked.tsv
grep -q 'meshkeys_raw' tests/generated_r22/linked.tsv
grep -q 'firstChild' tests/generated_r22/linked.dot
./ughtree tests/generated_r22/linked_cycle.ugh tests/generated_r22/linked_cycle.tsv
grep -q 'REVISIT_NODE' tests/generated_r22/linked_cycle.tsv
./ughtree tests/generated_r22/linked_bad_edge.ugh tests/generated_r22/linked_bad_edge.tsv
grep -q 'OUT_OF_BOUNDS' tests/generated_r22/linked_bad_edge.tsv
if ./ughtree tests/generated_r22/notugh.ugh >/dev/null 2>&1;then exit 1;fi
echo 'R22 linked UGH object walker tests PASS (synthetic; NOT native)' 
