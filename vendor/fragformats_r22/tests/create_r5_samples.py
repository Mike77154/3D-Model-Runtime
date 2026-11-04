from pathlib import Path
base=Path(__file__).parent
header='xof 0303txt 0032\n'
root='Frame Root {\n FrameTransformMatrix {1,0,0,0,0,1,0,0,0,0,1,0,4,5,6,1;;}\n'
child='Frame Child {\n FrameTransformMatrix {2,0,0,0,0,3,0,0,0,0,4,0,1,2,0,1;;}\n'
mesh='''Mesh Triangle {
3;
0;0;0;,
1;1;1;,
0;1;0;;
1;
3;0,1,2;;
MeshTextureCoords {3;0;0;,1;1;,0;1;;}
MeshNormals {1;0;0;1;;1;3;0,0,0;;}
}
'''
other='''Frame Other {
Mesh Another {
3; 1;0;0;,0;0;0;,0;1;0;;
1;3;0,1,2;;
}
}
'''
(base/'r5_nested_frame.x').write_text(header+root+child+mesh+'}\n'+other+'}\n')
(base/'r5_late_frame.x').write_text(header+'Frame Late {\n'+mesh+'FrameTransformMatrix {1,0,0,0,0,1,0,0,0,0,1,0,9,0,0,1;;}\n}\n')
(base/'r5_nonaffine.x').write_text(header+'Frame Perspective {\nFrameTransformMatrix {1,0,0,1,0,1,0,0,0,0,1,0,0,0,0,1;;}\n'+mesh+'}\n')
