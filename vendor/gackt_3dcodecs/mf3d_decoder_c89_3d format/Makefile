CC?=cc
CFLAGS?=-std=c89 -Wall -Wextra -pedantic -I./src -I./ext -I./ext/zip64_deflate -I./ext/zip_secure_span -I./ext/zip_crypto_aes
SRC=src/mf3d.c src/mf3d_zip.c src/mf3d_xml.c src/mf3d_fixed.c src/mf3d_inflate_store.c
EXT=ext/mf3d_ext_xml.c ext/mf3d_ext_materials.c ext/mf3d_ext_components.c ext/mf3d_ext_production.c ext/mf3d_ext_metadata.c ext/mf3d_ext_beam_lattice.c ext/mf3d_ext_slice.c ext/mf3d_ext_deflate_storeblock.c ext/mf3d_ext_opc.c ext/mf3d_ext_validate.c
ZIP64=ext/zip64_deflate/mf3d_zip64_deflate.c
SECSPAN=ext/zip_secure_span/mf3d_zip_secure_span.c
CRYPTOAES=ext/zip_crypto_aes/mf3d_zip_crypto_aes.c
CRYPTOGCM=ext/zip_crypto_aes/mf3d_zip_crypto_gcm.c
NEWEXT=ext/mf3d_ext_boolean.c ext/mf3d_ext_displacement.c ext/mf3d_ext_volumetric.c ext/mf3d_ext_secure_content.c ext/mf3d_ext_package.c ext/mf3d_ext_full.c
SECDEC=ext/mf3d_ext_decode_secure.c ext/mf3d_ext_opc.c ext/mf3d_ext_xml.c
HEAVY=ext/eval_heavy/mf3d_eval_mesh.c ext/eval_heavy/mf3d_eval_png.c ext/eval_heavy/mf3d_eval_displace.c ext/eval_heavy/mf3d_eval_implicit.c ext/eval_heavy/mf3d_eval_csg.c
FINALPUSH=ext/final_push/mf3d_final_repair.c ext/final_push/mf3d_final_beam_mesh.c ext/final_push/mf3d_final_slice.c ext/final_push/mf3d_final_texture.c ext/final_push/mf3d_final_export.c
all: mf3d_demo mf3d_ext_demo mf3d_zip64_demo mf3d_secure_decode_demo test_xml test_ext test_zip64_deflate test_zip_secure_span test_zip_crypto_aes test_secure_decode test_validate test_fullpush test_absence_eval test_final_push
mf3d_demo: examples/demo.c $(SRC)
	$(CC) $(CFLAGS) examples/demo.c $(SRC) -o mf3d_demo
mf3d_ext_demo: examples/ext_demo.c $(SRC) $(EXT)
	$(CC) $(CFLAGS) examples/ext_demo.c $(SRC) $(EXT) -o mf3d_ext_demo
test_xml: tests/test_xml.c $(SRC)
	$(CC) $(CFLAGS) tests/test_xml.c $(SRC) -o test_xml
test_ext: tests/test_ext.c $(SRC) $(EXT)
	$(CC) $(CFLAGS) tests/test_ext.c $(SRC) $(EXT) -o test_ext

mf3d_zip64_demo: examples/zip64_demo.c $(SRC) $(ZIP64)
	$(CC) $(CFLAGS) examples/zip64_demo.c $(SRC) $(ZIP64) -o mf3d_zip64_demo

mf3d_secure_decode_demo: examples/secure_decode_demo.c $(SRC) $(ZIP64) $(SECSPAN) $(CRYPTOAES) $(CRYPTOGCM) $(SECDEC)
	$(CC) $(CFLAGS) examples/secure_decode_demo.c $(SRC) $(ZIP64) $(SECSPAN) $(CRYPTOAES) $(CRYPTOGCM) $(SECDEC) -o mf3d_secure_decode_demo

test_zip64_deflate: tests/test_zip64_deflate.c $(ZIP64)
	$(CC) $(CFLAGS) tests/test_zip64_deflate.c $(ZIP64) -o test_zip64_deflate

test_zip_secure_span: tests/test_zip_secure_span.c $(ZIP64) $(SECSPAN)
	$(CC) $(CFLAGS) tests/test_zip_secure_span.c $(ZIP64) $(SECSPAN) -o test_zip_secure_span

test_zip_crypto_aes: tests/test_zip_crypto_aes.c $(CRYPTOAES) $(CRYPTOGCM)
	$(CC) $(CFLAGS) tests/test_zip_crypto_aes.c $(CRYPTOAES) $(CRYPTOGCM) -o test_zip_crypto_aes

test_secure_decode: tests/test_secure_decode.c $(SRC) $(ZIP64) $(SECSPAN) $(CRYPTOAES) $(CRYPTOGCM) $(SECDEC)
	$(CC) $(CFLAGS) tests/test_secure_decode.c $(SRC) $(ZIP64) $(SECSPAN) $(CRYPTOAES) $(CRYPTOGCM) $(SECDEC) -o test_secure_decode

test_validate: tests/test_validate.c $(SRC) $(EXT)
	$(CC) $(CFLAGS) tests/test_validate.c $(SRC) $(EXT) -o test_validate

test_fullpush: tests/test_fullpush.c $(SRC) $(EXT) $(NEWEXT) $(ZIP64) $(SECSPAN) $(CRYPTOAES) $(CRYPTOGCM) ext/mf3d_ext_decode_secure.c
	$(CC) $(CFLAGS) tests/test_fullpush.c $(SRC) $(EXT) $(NEWEXT) $(ZIP64) $(SECSPAN) $(CRYPTOAES) $(CRYPTOGCM) ext/mf3d_ext_decode_secure.c -o test_fullpush
clean:
	rm -f mf3d_demo mf3d_ext_demo mf3d_zip64_demo mf3d_secure_decode_demo test_xml test_ext test_zip64_deflate test_zip_secure_span test_zip_crypto_aes test_secure_decode test_validate test_fullpush test_absence_eval test_final_push test_final_push


test_absence_eval: tests/test_absence_eval.c $(SRC) $(EXT) ext/mf3d_ext_boolean.c ext/mf3d_ext_displacement.c $(ZIP64) $(HEAVY)
	$(CC) $(CFLAGS) -I. tests/test_absence_eval.c $(SRC) $(EXT) ext/mf3d_ext_boolean.c ext/mf3d_ext_displacement.c $(ZIP64) $(HEAVY) -o test_absence_eval


test_final_push: tests/test_final_push.c $(SRC) $(EXT) $(HEAVY) $(FINALPUSH)
	$(CC) $(CFLAGS) -I. tests/test_final_push.c $(SRC) $(EXT) $(HEAVY) $(FINALPUSH) -o test_final_push
