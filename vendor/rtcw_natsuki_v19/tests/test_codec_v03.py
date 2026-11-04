"""Original v0.3 fixtures; tests tag axes, two-piece assembly and frame blending."""
import pathlib, subprocess, struct, sys, tempfile
sys.path.insert(0,str(pathlib.Path(__file__).parent))
from test_codec import md3, mdc, mds, n
root=pathlib.Path(sys.argv[1]).resolve()
view=root/'rtcw_view';anim=root/'rtcw_anim';attach=root/'rtcw_attach'

def execute(cmd, success=True):
    p=subprocess.run([str(x) for x in cmd],capture_output=True,text=True)
    assert (p.returncode==0)==success,(p.returncode,p.stdout,p.stderr,cmd)
    return p

def vertices(obj):
    return [[float(q) for q in l.split()[1:]] for l in obj.splitlines() if l.startswith('v ')]

with tempfile.TemporaryDirectory() as d:
    t=pathlib.Path(d)
    for nm,contents in [('body.mds',mds()),('accessory.md3',md3()),('fangs.mdc',mdc())]:
        (t/nm).write_bytes(contents)
    # Rotation of the MDS tag follows the animated bone on frame 1.
    execute([view,t/'body.mds',t/'x.obj',t/'x.ppm',t/'mds.txt',1]);report=(t/'mds.txt').read_text()
    assert 'tag_axisQ14[0]' in report
    axis_line=[x for x in report.splitlines() if x.startswith('tag_axisQ14')][0]
    first=axis_line.partition('=')[2].split(';')[0]
    nums=[int(v) for v in first.strip('()').split(',')]
    assert abs(nums[0])<500 and nums[1]>15500,(nums,report)
    print('PASS MDS bone-based tag rotation follows animated yaw')
    # Attach MD3 accessory to the tag of an MDS at two distinct frames.
    execute([attach,t/'body.mds',0,'-','tag_head',t/'accessory.md3',0,'-',t/'combined.obj',t/'combined.ppm',t/'scene.txt'])
    combo=(t/'combined.obj').read_text()
    coords=vertices(combo)
    assert len(coords)==6 and combo.count('\nf ')==2 and 'g child_body' in combo
    assert 9.0<coords[3][0]<10.5,(coords[3],combo)
    assert (t/'combined.ppm').read_bytes().startswith(b'P6\n800 800\n255\n')
    print('PASS assembly joins body and accessory, preserves UV/triangles and PPM')
    execute([attach,t/'body.mds',1,'-','tag_head',t/'accessory.md3',0,'-',t/'combined2.obj',t/'combined2.ppm',t/'scene2.txt'])
    rot=vertices((t/'combined2.obj').read_text())
    assert abs(rot[4][0]-rot[3][0])<0.5 and rot[4][1]-rot[3][1]>9.0,(rot[3],rot[4])
    print('PASS attached accessory rotates with animated parent tag')
    # Missing tags must be rejected clearly.
    execute([attach,t/'body.mds',1,'-','tag_miss',t/'accessory.md3',0,'-',t/'bad.obj',t/'bad.ppm',t/'bad.txt'],False)
    print('PASS missing attachment tag rejected')
    # Lerp frame 0 1 halfway in MD3 from z 0 to 10.
    execute([anim,t/'accessory.md3',0,1,2048,t/'blend.obj',t/'blend.ppm',t/'blend.txt'])
    v=vertices((t/'blend.obj').read_text());assert abs(v[0][2]-5)<0.01,v[0]
    print('PASS MD3 midpoint interpolation')
    execute([anim,t/'body.mds',0,1,2048,t/'blend_mds.obj',t/'blend_mds.ppm',t/'blend_mds.txt'])
    b=vertices((t/'blend_mds.obj').read_text())
    assert 2.5<b[1][0]<3.3 and 6.7<b[1][1]<7.4,b[1]
    print('PASS MDS bone-space interpolated midpoint (changed in v0.4)')
    execute([anim,t/'accessory.md3',0,99,2048,t/'bad.obj',t/'bad.ppm',t/'bad.txt'],False)
    print('PASS invalid interpolation endpoint rejected')
    # MD3 tag axis parse: create one tag with a known 90 degree rotation.
    raw=bytearray(md3());hdr=bytearray(raw[:108]);frames=raw[108:220];surf=raw[220:]
    ax=(0.,1.,0.,-1.,0.,0.,0.,0.,1.)
    tag=n('tag_hand')+struct.pack('<3f',1,2,3)+struct.pack('<9f',*ax)
    tag2=n('tag_hand')+struct.pack('<3f',1,2,3)+struct.pack('<9f',*ax)
    struct.pack_into('<I',hdr,80,1)
    struct.pack_into('<I',hdr,100,108+len(frames)+len(tag+tag2))
    struct.pack_into('<I',hdr,104,108+len(frames)+len(tag+tag2)+len(surf))
    (t/'parent.md3').write_bytes(hdr+frames+tag+tag2+surf)
    execute([view,t/'parent.md3',t/'x.obj',t/'x.ppm',t/'md3.txt',0]);data=(t/'md3.txt').read_text()
    assert 'tag_axisQ14[0]=(0,16384,0);(-16384,0,0);(0,0,16384)' in data,data
    execute([attach,t/'parent.md3',0,'-','tag_hand',t/'accessory.md3',0,'-',t/'m.obj',t/'m.ppm',t/'m.txt'])
    r=vertices((t/'m.obj').read_text());assert abs(r[4][0]-r[3][0])<0.02 and r[4][1]-r[3][1]>9.99,(r[3],r[4])
    print('PASS MD3 tag rotation and MD3-to-MD3 attachment')
    # MDC tag angles are not binary turn units, they use /32700 steps.
    raw=bytearray(mdc());hdr=bytearray(raw[:112]);frames=raw[112:224];surf=raw[224:]
    tags=n('tag_arm');t0=struct.pack('<6h',0,0,0,0,0,0);t1=struct.pack('<6h',0,0,0,0,8175,0)
    struct.pack_into('<I',hdr,80,1)
    struct.pack_into('<I',hdr,100,112+len(frames)+64)
    struct.pack_into('<I',hdr,104,112+len(frames)+64+24)
    struct.pack_into('<I',hdr,108,112+len(frames)+64+24+len(surf))
    (t/'tag.mdc').write_bytes(hdr+frames+tags+t0+t1+surf)
    execute([view,t/'tag.mdc',t/'x.obj',t/'x.ppm',t/'mdc.txt',1]);data=(t/'mdc.txt').read_text()
    ax=[x for x in data.splitlines() if x.startswith('tag_axisQ14')][0].split('=')[1].split(';')[0]
    nums=[int(v) for v in ax.strip('()').split(',')]
    assert abs(nums[0])<500 and nums[1]>15500,nums
    print('PASS MDC compressed tag angles use 32700-step turns')
print('ALL v0.3 TESTS PASS')
