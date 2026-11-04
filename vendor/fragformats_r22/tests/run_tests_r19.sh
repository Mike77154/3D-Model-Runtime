#!/bin/sh
set -eu
mkdir -p tests/generated_r19
python3 - <<'PY'
from pathlib import Path
p=Path('tests/generated_r19')
p.joinpath('base.ugh').write_bytes(b'ugh0'+bytes(12)+b'geometry\x00mesh\x00' + b'\0'*6 + 'Bone1'.encode('utf-16le')+b'\0\0')
p.joinpath('edit.ugh').write_bytes(b'ugh0'+bytes(12)+b'geometry\x00mesh\x00' + b'\0'*6 + 'Bone2'.encode('utf-16le')+b'\0\0')
p.joinpath('short.ugh').write_bytes(b'ugh0')
p.joinpath('bad.ugh').write_bytes(b'UGHH'+bytes(12)+b'geometry')
PY
./fraginspect tests/generated_r19/base.ugh tests/generated_r19/scan.tsv
./ughcompare tests/generated_r19/base.ugh tests/generated_r19/edit.ugh tests/generated_r19/diff.tsv
 grep -q 'candidate_ascii' tests/generated_r19/scan.tsv
 grep -q 'candidate_utf16le' tests/generated_r19/scan.tsv
 grep -q 'changed_range' tests/generated_r19/diff.tsv
 if ./fraginspect tests/generated_r19/short.ugh >/dev/null 2>&1;then echo 'accepted too-short UGH' >&2;exit 1;fi
 if ./fraginspect tests/generated_r19/bad.ugh >/dev/null 2>&1;then echo 'accepted malformed UGH' >&2;exit 1;fi
 if ./ughcompare tests/generated_r19/bad.ugh tests/generated_r19/edit.ugh >/dev/null 2>&1;then echo 'accepted malformed compare input' >&2;exit 1;fi
 echo 'R19 UGH diagnostics tests PASS (synthetic fixtures only)'
