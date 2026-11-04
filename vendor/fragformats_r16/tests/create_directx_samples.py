#!/usr/bin/env python3
"""Deterministic text and binary DirectX X fixtures, plus corruption cases."""
import os,struct
root=os.path.dirname(__file__)
def write(n,b):
 with open(os.path.join(root,n), 'wb') as f:f.write(b)
def header(kind='txt ',flt='0032',v='0303'):
 return ('xof '+v+kind+flt).encode('ascii')
# Triangle + quad, normals, UVs, face material, skin, nested frame and animation
src='''
// End to end text fixture, also tests n-gon triangulation
Header {1;0;1;}
Frame Root {
 FrameTransformMatrix { 1,0,0,0, 0,1,0,0, 0,0,1,0, 4,5,6,1;; }
 Mesh TestMesh {
 4;
 0;0;0;,
 1;0;0;,
 1;1;0;,
 0;1;0;;
 2;
 3;0,1,2;,
 4;0,1,2,3;;
 MeshNormals { 1; 0;0;1;; 2; 3;0,0,0;,4;0,0,0,0;; }
 MeshTextureCoords { 4;0;0;,1;0;,1;1;,0;1;; }
 MeshMaterialList {
 1;2;0,0;;
 Material red { 1;0.2;0.3;1;; 12; 0;0;0;;0;0;0;; TextureFilename { "skin.png"; } }
 }
 SkinWeights { "Root"; 2; 0,1; 0.25,0.75; 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1;; }
 }
}
AnimTicksPerSecond { 4800; }
AnimationSet Walk { Animation Clip { { Root } AnimationKey { 2; 2; 0;3; 0;0;0;;, 10;3; 1;2;3;;; } } }
'''
write('directx_txt32.x',header()+src.encode())
write('directx_txt64.x',header(flt='0064')+src.replace('1;0;0;,','1.1e0;0;0;,',1).encode())
# 0302 without children
write('directx_multimesh.x',header()+b'Mesh Left {3;0;0;0;,1;0;0;,0;1;0;;1;3;0,1,2;; MeshTextureCoords {3;0;0;,1;0;,0;1;;}} Mesh Right {3;2;0;0;,3;0;0;,2;1;0;;1;3;0,1,2;;MeshNormals{1;0;0;1;;1;3;0,0,0;;}}')
write('directx_txt0302.x', header(v='0302')+b' Mesh M {3;0;0;0;,1;0;0;,0;1;0;;1;3;0,1,2;;}')
# binary tokens
h=lambda n:struct.pack('<H',n)
def nm(s):s=s.encode('ascii');return h(1)+struct.pack('<I',len(s))+s
def st(s):s=s.encode('ascii');return h(2)+struct.pack('<I',len(s))+s
def li(*items):return h(6)+struct.pack('<I',len(items))+struct.pack('<%dI'%len(items),*items)
def fl(flt,*items):return h(7)+struct.pack('<I',len(items))+struct.pack('<%d%s'%(len(items),'f' if flt==32 else 'd'),*items)
def tok(n):return h(n)
for fw in (32,64):
 data=(nm('Mesh')+nm('BinaryMesh')+tok(10)+li(3)+fl(fw,0,0,0,2,0,0,0,2,0)+li(1,3,0,1,2)+
       nm('MeshNormals')+tok(10)+li(1)+fl(fw,0,0,1)+li(1,3,0,0,0)+tok(11)+
       nm('MeshTextureCoords')+tok(10)+li(3)+fl(fw,0,0,1,0,0,1)+tok(11)+tok(11))
 write('directx_bin%d.x'%fw,header(kind='bin ',flt='0032' if fw==32 else '0064')+data)
# Bad input
write('bad_directx_truncated.x',header()+b'Mesh A { 3; 0;0;0;')
write('bad_directx_indices.x',header()+b'Mesh A {3;0;0;0;,1;0;0;,0;1;0;;1;3;0,1,9;;}')
write('bad_directx_compressed.x',header(kind='tzip')+b'abc')
write('bad_directx_oversized.x',header()+b'Mesh A { 100001; }')
write('bad_directx_bin_truncated.x',header(kind='bin ')+nm('Mesh')+tok(10)+li(3)+fl(32,0,0))
print('Generated DirectX X test fixtures')

# Binary string terminators: both WORD and DWORD forms must work.
for term in (2,4):
 tail=st('Root')+tok(20)+(b'\0\0' if term==4 else b'')
 binary=(nm('Mesh')+nm('SkinMesh')+tok(10)+li(3)+fl(32,0,0,0,1,0,0,0,1,0)+li(1,3,0,1,2)+nm('SkinWeights')+tok(10)+tail+li(1,0)+fl(32,1)+fl(32,*([1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]))+tok(11)+tok(11))
 write('directx_bin_string%d.x'%term,header(kind='bin ')+binary)
