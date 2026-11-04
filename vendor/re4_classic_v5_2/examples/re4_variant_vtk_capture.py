#!/usr/bin/env python3
"""VTK diagnostic for OBJ/MTL poses produced by C89 re4_character_clip.

Extract only referenced original TGA files from a caller-provided GCA archive.
All proprietary inputs stay outside the redistributable C89 source package.
No character identity inference, animation emulation, or image generation.
"""
import argparse
from pathlib import Path
import re
from re4_all_characters import indexes, read_at
from re4_vtk_multipart import render
from PIL import Image, ImageDraw


def textures_for_pose(poses, archive, catalog, family):
    names = set()
    for mtl in poses.glob('frame_*.obj.mtl'):
        names.update(re.findall(r'^map_Kd\s+(.+)$', mtl.read_text(errors='replace'), re.M))
    directory = 'xfile\\em\\' + family.lower() + '\\'
    indexed = {e['name'].lower(): e for e in catalog}
    outputs = []
    with archive.open('rb') as src:
        for name in sorted(names):
            key = (directory + name).lower()
            e = indexed.get(key)
            if e is None:
                outputs.append((name, 'MISSING')); continue
            target = poses / name
            target.write_bytes(read_at(src, e, cap=32 * 1024 * 1024))
            outputs.append((name, 'FOUND'))
    return outputs


def capture(args):
    cat = indexes((Path(args.codec) / 're4_gca_catalog').resolve(), args.xfile)
    out = Path(args.out); out.mkdir(parents=True, exist_ok=True)
    imgs = []
    for family in args.families.split(','):
        poses = out / family
        # Family keys in archive are pl01/pl05, while local folder names can be custom.
        pmdfamily = args.model_families.split(',')[args.families.split(',').index(family)]
        status = textures_for_pose(poses, Path(args.xfile), cat, pmdfamily)
        print(family, 'textures:', status, flush=True)
        for frame in map(int, args.frames.split(',')):
            path = poses / f'frame_{frame:05d}.obj'
            png = poses / f'vtk_{frame:05d}.png'
            actors, bounds = render(path, png)
            print('RENDER', family, frame, actors, bounds, flush=True)
            pic=Image.open(png).convert('RGB');
            draw=ImageDraw.Draw(pic)
            draw.rectangle((0,0,720,42),fill='#172332')
            draw.text((18,12),f'{family.upper()} | FCV frame {frame:03d} | VTK',fill='#ffffff')
            imgs.append(pic)
    nframe=len(args.frames.split(',')); cols=nframe;rows=len(imgs)//cols
    sheet=Image.new('RGB',(720*cols,780*rows),(15,20,28))
    for n,img in enumerate(imgs):sheet.paste(img,((n%cols)*720,(n//cols)*780))
    sheet.save(out/'RE4_ASHLEY_VTK_MULTIVARIANT.png')
    print('MONTAGE',out/'RE4_ASHLEY_VTK_MULTIVARIANT.png')

if __name__ == '__main__':
    p=argparse.ArgumentParser()
    p.add_argument('--codec',required=True);p.add_argument('--xfile',required=True)
    p.add_argument('--out',required=True);p.add_argument('--families',default='ashley01,ashley05')
    p.add_argument('--model-families',default='pl01,pl05');p.add_argument('--frames',default='0,3,6')
    capture(p.parse_args())
