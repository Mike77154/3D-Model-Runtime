xof 0303txt 0032
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
