"""Generate C89 runtime bridge fixtures from the vendored RTCW test generator.
This script is a developer-only test utility, not a runtime dependency.
Usage: python3 tests/generate_rtcw_re4_fixture.py [output_dir]
"""
from pathlib import Path
import sys, runpy
out=Path(sys.argv[1] if len(sys.argv)>1 else 'tests/rtcw_re4_fixtures')
out.mkdir(parents=True,exist_ok=True)
gen=runpy.run_path('vendor/rtcw_natsuki_v19/tests/test_codec.py')
for extension in ('mds','mdc','md3'):
    (out/('synthetic.'+extension)).write_bytes(gen[extension]())
(out/'synthetic.seq').write_bytes(bytes([1,0,0,0,5,0,64,0]))
print('Created MDS/MDC/MD3 and SEQ synthetic fixtures in',out)
