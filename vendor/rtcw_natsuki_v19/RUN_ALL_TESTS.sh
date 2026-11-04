#!/bin/sh
set -eu
python3 tests/test_codec.py ./rtcw_view
python3 tests/test_codec_v02.py ./rtcw_view
python3 tests/test_codec_v03.py .
python3 tests/test_codec_v04.py .
python3 tests/test_codec_v05.py .
python3 tests/test_codec_v06.py ./rtcw_pk3 ./rtcw_view ./rtcw_texview
python3 tests/test_codec_v07.py ./rtcw_family
python3 tests/test_codec_v08.py ./rtcw_vfs ./rtcw_family
python3 tests/test_codec_v09.py ./rtcw_clips
python3 tests/test_codec_v10.py ./rtcw_script
python3 tests/test_codec_v11.py ./rtcw_preview
python3 tests/test_codec_v12.py ./rtcw_rig
python3 tests/test_codec_v13.py ./rtcw_rig
python3 tests/test_codec_v14.py ./rtcw_rig
python3 tests/test_codec_v14_images.py ./rtcw_imagecheck
python3 tests/test_codec_v15.py ./rtcw_rig ./rtcw_matprobe
python3 tests/test_codec_v16.py ./rtcw_shaderprobe "${CC:-gcc}"
python3 tests/test_codec_v17.py "${CC:-gcc}"
python3 tests/test_codec_v18.py ./rtcw_rig
python3 tests/test_codec_v19.py ./rtcw_audit
