import gzip
import pathlib
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / 'blendc89'
CORPUS = Path('/mnt/data/_blend_real_corpus/blender-2.79a/release__datafiles__startup.blend')

def call(*args):
    return subprocess.run([str(EXE), *map(str,args)],capture_output=True,text=True,timeout=40)

class Phase08(unittest.TestCase):
    def test_gzip_and_zstd(self):
        if not CORPUS.exists():self.skipTest('authentic sample not available')
        with tempfile.TemporaryDirectory() as t:
            t=Path(t)
            src=CORPUS.read_bytes()
            raw=t/'original.blend';raw.write_bytes(src)
            gz=t/'scene.gz';gz.write_bytes(gzip.compress(src))
            zst=t/'scene.zst';subprocess.run(['zstd','-q','-f','-o',str(zst),str(raw)],check=True)
            for name in [raw,gz,zst]:
                result=call('--info',name)
                self.assertEqual(result.returncode,0,msg=(name.name,result.stderr))
                self.assertIn('BLEND version: 272',result.stdout)
                self.assertIn('Blocks: 842 ',result.stdout)
            # Corrupt checksum/truncated files rejected.
            bad=t/'bad.gz';bad.write_bytes(gz.read_bytes()[:-5]);self.assertNotEqual(call('--info',bad).returncode,0)
            badz=t/'bad.zst';badz.write_bytes(zst.read_bytes()[:-5]);self.assertNotEqual(call('--info',badz).returncode,0)
    def test_direct_blend_frame(self):
        from test_phase04_animation import Fixture
        f=Fixture(ptr=4)
        f.add('Bone',[('Bone','*next'),('Bone','*prev'),('Bone','*parent'),
                      ('ListBase','childbase'),('char','name[64]'),
                      ('float','arm_mat[4][4]')])
        f.add('bArmature',[('ListBase','bonebase')])
        f.size('Bone');f.size('bArmature')
        ident=[1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1]
        f.block(b'AR\0\0',f.obj('bArmature',{'bonebase':f.packptr(0x1000)+f.packptr(0x1000)}),0x900,'bArmature')
        f.block(b'DATA',f.obj('Bone',{'name':b'Root\0','arm_mat':ident}),0x1000,'Bone')
        f.block(b'AC\0\0',f.obj('bAction',{'curves':f.packptr(0x300)+f.packptr(0x300)}),0x200,'bAction')
        f.block(b'DATA',f.obj('FCurve',{'fpt':0x500,'totvert':2,'array_index':0,'rna_path':0x800}),0x300,'FCurve')
        f.block(b'DATA',f.obj('FPoint',{'vec':[1,2.5],'flag':0})+
                         f.obj('FPoint',{'vec':[9,7],'flag':0}),0x500,'FPoint',2)
        f.block(b'DATA',b'pose.bones["Root"].location\0',0x800)
        with tempfile.TemporaryDirectory() as t:
            t=Path(t)
            raw=t/'skinned.blend';raw.write_bytes(f.build())
            gz=t/'skinned.gz';gz.write_bytes(gzip.compress(raw.read_bytes()))
            zs=t/'skinned.zst';subprocess.run(['zstd','-q','-f','-o',str(zs),str(raw)],check=True)
            verts=t/'verts.tsv';verts.write_text('0 1 0 0 1 0 1\n')
            out=t/'out.tsv'
            for sample in [raw,gz,zs]:
                proc=call('--play-blend',sample,'5',verts,out)
                self.assertEqual(proc.returncode,0,(sample.name,proc.stderr))
                rows=[line.split() for line in out.read_text().splitlines() if line and line[0]!='#']
                self.assertEqual(len(rows),1)
                self.assertAlmostEqual(float(rows[0][1]),5.75,delta=0.001)
                self.assertAlmostEqual(float(rows[0][2]),0,delta=0.001)
    def test_auto_obj_animation(self):
        from test_phase04_animation import Fixture
        f=Fixture(ptr=4)
        f.add('Bone',[('Bone','*next'),('Bone','*prev'),('Bone','*parent'),
                      ('ListBase','childbase'),('char','name[64]'),('float','arm_mat[4][4]')])
        f.add('bArmature',[('ListBase','bonebase')])
        f.add('Mesh',[('MVert','*mvert'),('MFace','*mface'),('int','totvert'),('int','totface'),
                      ('MDeformVert','*dvert'),('ListBase','vertex_group_names')])
        f.add('MVert',[('float','co[3]')])
        f.add('MFace',[('int','v1'),('int','v2'),('int','v3'),('int','v4')])
        f.add('MDeformVert',[('MDeformWeight','*dw'),('int','totweight')])
        f.add('MDeformWeight',[('int','def_nr'),('float','weight')])
        f.add('bDeformGroup',[('bDeformGroup','*next'),('bDeformGroup','*prev'),('char','name[64]')])
        for n in ('Bone','bArmature','Mesh','MVert','MFace','MDeformVert','MDeformWeight','bDeformGroup'):f.size(n)
        ident=[1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1]
        f.block(b'AR\0\0',f.obj('bArmature',{'bonebase':f.packptr(0x1000)+f.packptr(0x1000)}),0x900,'bArmature')
        f.block(b'DATA',f.obj('Bone',{'name':b'Root\0','arm_mat':ident}),0x1000,'Bone')
        f.block(b'AC\0\0',f.obj('bAction',{'curves':f.packptr(0x300)+f.packptr(0x300)}),0x200,'bAction')
        f.block(b'DATA',f.obj('FCurve',{'fpt':0x500,'totvert':2,'array_index':0,'rna_path':0x800}),0x300,'FCurve')
        f.block(b'DATA',f.obj('FPoint',{'vec':[1,2.5],'flag':0})+f.obj('FPoint',{'vec':[9,7],'flag':0}),0x500,'FPoint',2)
        f.block(b'DATA',b'pose.bones["Root"].location\0',0x800)
        f.block(b'ME\0\0',f.obj('Mesh',{'mvert':0x2000,'mface':0x2100,'totvert':3,'totface':1,
                          'dvert':0x2200,'vertex_group_names':f.packptr(0x2500)+f.packptr(0x2500)}),0x2600,'Mesh')
        verts=b''.join(f.obj('MVert',{'co':co}) for co in ([1,0,0],[0,1,0],[0,0,0]))
        f.block(b'DATA',verts,0x2000,'MVert',3)
        f.block(b'DATA',f.obj('MFace',{'v1':0,'v2':1,'v3':2,'v4':0}),0x2100,'MFace')
        stride=f.size('MDeformWeight')
        dvert=b''.join(f.obj('MDeformVert',{'dw':0x2300+i*stride,'totweight':1}) for i in range(3))
        f.block(b'DATA',dvert,0x2200,'MDeformVert',3)
        f.block(b'DATA',b''.join(f.obj('MDeformWeight',{'def_nr':0,'weight':[1]}) for _ in range(3)),0x2300,'MDeformWeight',3)
        f.block(b'DATA',f.obj('bDeformGroup',{'name':b'Root\0'}),0x2500,'bDeformGroup')
        with tempfile.TemporaryDirectory() as t:
            t=Path(t);src=t/'animated.blend';src.write_bytes(f.build())
            zipped=t/'animated.gz';zipped.write_bytes(gzip.compress(src.read_bytes()))
            zstd=t/'animated.zst';subprocess.run(['zstd','-q','-f','-o',str(zstd),str(src)],check=True)
            for source in (src,zipped,zstd):
                target=t/'animated.obj'
                proc=call('--play-obj',source,'5',target)
                self.assertEqual(proc.returncode,0,(source,proc.stdout,proc.stderr))
                result=target.read_text()
                self.assertIn('v 5.7500 0.0000 0.0000',result)
                self.assertIn('f 1 2 3',result)
            seq=call('--play-sequence',src,'3','5',t/'frame')
            self.assertEqual(seq.returncode,0,seq.stderr)
            self.assertEqual(len(list(t.glob('frame_frame*.obj'))),3)
    def test_rig_playback(self):
        with tempfile.TemporaryDirectory() as t:
            t=Path(t)
            rest=t/'rest.tsv';sam=t/'sample.tsv';verts=t/'vertices.tsv';out=t/'output.tsv'
            ident=[1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1]
            child=[1,0,0,1, 0,1,0,0, 0,0,1,0, 0,0,0,1]
            rest.write_text('0 0 -1 Root '+' '.join(map(str,ident))+'\n'+
                            '0 1 0 Child '+' '.join(map(str,child))+'\n')
            sam.write_text('0\t0\t0\t0\t0\tpose.bones["Root"].location\t0\t5.0\t1.0\n'+
                           '0\t0\t0\t0\t1\tpose.bones["Child"].rotation_quaternion\t0\t5.0\t0.7071\n'+
                           '0\t0\t0\t0\t2\tpose.bones["Child"].rotation_quaternion\t3\t5.0\t0.7071\n')
            verts.write_text('0 2 0 0 1 1 1\n1 0 0 0 1 0 1\n')
            result=call('--animate-tsv',rest,sam,verts,out)
            self.assertEqual(result.returncode,0,result.stderr)
            rows=[line.split() for line in out.read_text().splitlines() if line and line[0]!='#']
            self.assertEqual(len(rows),2)
            p=list(map(float,rows[0][1:]));self.assertAlmostEqual(p[0],2,delta=0.002);self.assertAlmostEqual(p[1],1,delta=0.002)
            p=list(map(float,rows[1][1:]));self.assertAlmostEqual(p[0],1,delta=0.002)
            dup=t/'bad_samples.tsv';dup.write_text(sam.read_text()+sam.read_text().splitlines()[0]+'\n')
            self.assertNotEqual(call('--animate-tsv',rest,dup,verts,out).returncode,0)
            self.assertTrue(out.exists())
            self.assertEqual(len([r for r in out.read_text().splitlines() if r and r[0]!="#"]),2)
if __name__=='__main__':unittest.main()

# Standalone unittest discovery does not need imported side effects; this suite
# builds an independent tiny SDNA armature plus legacy Action/FCurve fixture.
