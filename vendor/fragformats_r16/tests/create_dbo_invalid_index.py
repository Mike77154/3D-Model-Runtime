"""Mutate generated synthetic DBO indices only, no copyrighted source models."""
import pathlib,struct
P=pathlib.Path(__file__).resolve().parent
b=bytearray((P/'synthetic_plane.dbo').read_bytes())
# Locate sub-block 116 (index buffer) by verified expected sequence, not global blind replace.
needle=struct.pack('<II6H',116,12,0,1,2,0,2,3)
i=b.find(needle)
assert i>=0
b[i+8:i+10]=struct.pack('<H',65535)
(P/'bad_index_plane.dbo').write_bytes(b)
