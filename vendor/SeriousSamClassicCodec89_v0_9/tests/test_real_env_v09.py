#!/usr/bin/env python3
"""Optional original-asset model/material smoke test.
Reads the user's separately supplied Serious Engine archive; does not store
original assets in this source package. Linux/MINGW binary extension is probed.
"""
import io, sys, tempfile, subprocess, zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def run(*args):
    r=subprocess.run(list(map(str,args)),capture_output=True,text=True)
    if r.returncode: raise RuntimeError((args,r.stdout,r.stderr))
    return r.stdout
def main():
    if len(sys.argv)!=2:
        print('usage: python3 tests/test_real_env_v09.py Serious-Engine-master.zip');return 2
    path=Path(sys.argv[1])
    e=lambda name: ROOT/'bin'/name if (ROOT/'bin'/name).exists() else ROOT/'bin'/(name+'.exe')
    with zipfile.ZipFile(path) as source, tempfile.TemporaryDirectory(prefix='ss89_v09_real_') as temp:
        d=Path(temp); asset=d/'assets'
        game=next(name for name in source.namelist() if name.endswith('/SE1_10.gro'))
        with zipfile.ZipFile(io.BytesIO(source.read(game))) as gro:
            names=['Models/Enemies/Headman/Projectile/Blade.mdl',
                   'Models/Enemies/Headman/Projectile/Blade.tex',
                   'Models/Enemies/Boneman/Boneman.mdl',
                   'Models/Enemies/Boneman/Boneman.tex',
                   'Models/SpecularTextures/Medium.tex',
                   'Models/ReflectionTextures/LightBlueMetal01.tex']
            for name in names:
                out=asset/name;out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(gro.read(name))
        filename=next(x for x in source.namelist() if x.endswith('Sources/Models/Enemies/Headman/Projectile/Blade.ini'))
        (d/'Blade.ini').write_bytes(source.read(filename))
        run(sys.executable,ROOT/'tools/make_gouraud89.py','--engine-zip',path,d/'gouraud.q15')
        for item,stem,flag in [('Blade','Blade','1'),('Boneman','Kleer','0')]:
            if item=='Blade':name='Models/Enemies/Headman/Projectile/Blade'
            else:name='Models/Enemies/Boneman/Boneman'
            obj=d/(stem+'.obj')
            run(e('ss89scene'),'unused.amc','--root',asset,'--model',name+'.mdl',
                '--texture',name+'.tex','--frame','0',
                '--gouraud-table',d/'gouraud.q15','--obj',obj)
            # render with explicit maps; always gated by original surface flags
            base=d/(stem+'_base.ppm'); lit=d/(stem+'_lit.ppm')
            run(e('ss89present'),obj,base,'145','--filter','bilinear')
            if item=='Blade':
                run(e('ss89present'),obj,lit,'145','--filter','bilinear',
                    '--settings',d/'Blade.ini','--assets-root',asset)
            else:
                # use .tex through the original-resource converter
                run(e('ss89resource'),asset/'Models/SpecularTextures/Medium.tex','--pam',d/'BladeSpec.pam')
                run(e('ss89resource'),asset/'Models/ReflectionTextures/LightBlueMetal01.tex','--pam',d/'BladeRefl.pam')
                run(e('ss89present'),obj,lit,'145','--filter','bilinear',
                    '--spec-map',d/'BladeSpec.pam','--reflection-map',d/'BladeRefl.pam')
            a,b=base.read_bytes(),lit.read_bytes()
            print(f'ORIGINAL_{stem}: bytes_changed={sum(x!=y for x,y in zip(a,b))}, expected={flag}')
            assert (a!=b)==(flag=='1'),stem
    print('V09 REAL ASSET PASS: original WTEX mapping, original Blade reflect/spec; Kleer remains unmodified')
    return 0
if __name__=='__main__':sys.exit(main())
