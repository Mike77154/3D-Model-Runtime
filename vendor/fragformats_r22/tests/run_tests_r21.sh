#!/bin/sh
set -eu
mkdir -p tests/generated_r21
python3 - <<'PY'
from pathlib import Path
p=Path('tests/generated_r21'); base=bytearray(b'ugh0'+bytes(12))
# A deterministic nontrivial stream; deliberately NOT a native UGH model.
base += bytes(((i*37+i//7*11)%251) for i in range(700))
p.joinpath('base.ugh').write_bytes(base)
# One 9-byte insert, one 4-byte replacement and one 6-byte deletion.
edit=base[:140]+b'BONE-XYZ!'+base[140:]
edit=edit[:310]+b'UVTX'+edit[314:]
edit=edit[:521]+edit[527:]
p.joinpath('edited.ugh').write_bytes(edit)
short=b'ugh0'+bytes(12)+b'A'
p.joinpath('short.ugh').write_bytes(short)
p.joinpath('invalid.ugh').write_bytes(b'NOPE'+bytes(30))
PY
./ughresync tests/generated_r21/base.ugh tests/generated_r21/edited.ugh tests/generated_r21/resync.tsv
./ughcompare tests/generated_r21/base.ugh tests/generated_r21/edited.ugh tests/generated_r21/naive.tsv
 grep -q '^insert\s' tests/generated_r21/resync.tsv
 grep -q '^delete\s' tests/generated_r21/resync.tsv
 grep -q '^replace\s' tests/generated_r21/resync.tsv
 if ./ughresync tests/generated_r21/base.ugh tests/generated_r21/invalid.ugh >/dev/null 2>&1;then echo 'Invalid UGH was accepted' >&2;exit 1;fi
 ./ughresync tests/generated_r21/base.ugh tests/generated_r21/short.ugh tests/generated_r21/tail.tsv
 grep -q '^opaque_replace_tail' tests/generated_r21/tail.tsv
 echo 'R21 aligned UGH comparator tests PASS (opaque synthetic streams only)'
python3 - <<'PYTHON_GEN'
from pathlib import Path
import struct
p=Path('tests/generated_r21')
# SDK-defined record shapes, but candidate placement is NOT verified in native files.
header=struct.pack('<4sIII', b'ugh0', 0x10000, 16, 2)
rec0=struct.pack('<7I', 72, 1, 80, 0, 1, 0xffffffff, 0)
rec1=struct.pack('<7I', 88, 0, 0, 0, 0xffffffff, 0xffffffff, 0)
p.joinpath('candidate_structured.ugh').write_bytes(header+rec0+rec1+bytes(160))
p.joinpath('candidate_bad_table.ugh').write_bytes(struct.pack('<4sIII',b'ugh0',0x10000,16,0xffffffff)+bytes(8))
PYTHON_GEN
./ughstructures tests/generated_r21/candidate_structured.ugh tests/generated_r21/structure.tsv
grep -q 'CANDIDATE_CONTIGUOUS_OBJECT_RECORDS' tests/generated_r21/structure.tsv
grep -q 'object_numAttr' tests/generated_r21/structure.tsv
./ughstructures tests/generated_r21/candidate_bad_table.ugh tests/generated_r21/bad_structure.tsv
grep -q 'NOT_PLAUSIBLE' tests/generated_r21/bad_structure.tsv
if ./ughstructures tests/generated_r21/invalid.ugh >/dev/null 2>&1;then echo 'invalid header accepted' >&2;exit 1;fi
echo 'R21 SDK object-record candidate tests PASS (synthetic)' 
