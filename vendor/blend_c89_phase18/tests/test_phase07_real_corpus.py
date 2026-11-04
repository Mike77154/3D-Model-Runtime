"""Optional integration test against unmodified genuine .blend files from source archives.
Usage: python3 tests/test_phase07_real_corpus.py /path/to/_blend_real_corpus
The Blender files are external and intentionally NOT redistributed in this project.
"""
import pathlib,subprocess,tempfile,sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
EXE=ROOT/'blendc89'
CASES=[
('blender-2.58','release__datafiles__preview.blend',250,2446,2310,0),
('blender-2.58','source__gameengine__GamePlayer__common__load.blend',241,4,1,4),
('blender-2.79a','release__datafiles__startup.blend',272,8,6,0),
('blender-2.79a','release__datafiles__preview.blend',262,1345,1914,6344),
('blender-2.90.1','release__datafiles__startup.blend',280,8,6,24),
('blender-3.6.3','release__datafiles__startup.blend',306,8,6,24),
('blender-4.5.14','release__datafiles__preview_grease_pencil.blend',403,211,121,484),
]
def check(root):
    for folder,name,version,verts,faces,uv in CASES:
        src=root/folder/name
        if not src.exists():raise FileNotFoundError(src)
        inf=subprocess.run([str(EXE),'--info',str(src)],capture_output=True,text=True,timeout=10)
        assert inf.returncode==0,(folder,name,inf.stderr)
        assert f'BLEND version: {version}' in inf.stdout,inf.stdout
        with tempfile.TemporaryDirectory() as td:
            target=pathlib.Path(td)/'out.obj'
            r=subprocess.run([str(EXE),'--obj',str(src),str(target)],capture_output=True,text=True,timeout=15)
            assert r.returncode==0,(folder,name,r.stderr)
            counts=[0,0,0]
            with target.open('r') as fp:
                for line in fp:
                    if line.startswith('v '):counts[0]+=1
                    if line.startswith('f '):counts[1]+=1
                    if line.startswith('vt '):counts[2]+=1
            assert tuple(counts)==(verts,faces,uv),(folder,name,counts)
            print('PASS real source asset',folder,name,'vertices/faces/uv',*counts)
if __name__=='__main__':
    if len(sys.argv)!=2:
        raise SystemExit('Usage: python3 tests/test_phase07_real_corpus.py CORPUS_DIRECTORY')
    check(pathlib.Path(sys.argv[1]))
