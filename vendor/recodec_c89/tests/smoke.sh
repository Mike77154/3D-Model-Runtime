#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
make
./build/grc info samples/cube.obj
./build/grc info samples/two_piece.obj
./build/grc convert samples/two_piece.obj build/two_piece.gfm --normals
./build/grc info build/two_piece.gfm
./build/grc convert build/two_piece.gfm build/two_piece.obj
./build/grc validate samples/cube.obj
./build/grc convert samples/cube.obj build/cube.gfm --clean --normals
./build/grc validate build/cube.gfm
./build/grc convert build/cube.gfm build/cube.obj
./build/grc clean build/cube.gfm build/cube_clean.obj --triangulate --normals --weld 0.0001
./build/grc convert build/cube.gfm build/cube.tmd --psx
./build/grc info build/cube.tmd
./build/grc convert build/cube.gfm build/cube.md1 --psx
./build/grc info build/cube.md1
./build/grc convert build/cube.gfm build/cube.md2 --psx
./build/grc info build/cube.md2
./build/grc convert build/cube.gfm build/cube_re1.emd --game re1 --psx
./build/grc info build/cube_re1.emd
./build/grc sections build/cube_re1.emd
./build/grc convert build/cube.gfm build/cube_re2.emd --game re2 --psx
./build/grc info build/cube_re2.emd
./build/grc sections build/cube_re2.emd
./build/grc convert build/cube.gfm build/cube_re3.emd --game re3 --psx
./build/grc info build/cube_re3.emd
./build/grc sections build/cube_re3.emd
./build/grc convert build/cube.gfm build/cube.pld --game re3 --psx
./build/grc info build/cube.pld
./build/giffy_re_view build/cube.gfm build/cube_preview.ppm 320 240 xy
./build/giffy_re_view build/cube.gfm build/cube_preview_xz.ppm 320 240 xz
echo "smoke ok"
