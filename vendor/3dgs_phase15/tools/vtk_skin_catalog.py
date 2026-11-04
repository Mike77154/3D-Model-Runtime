#!/usr/bin/env python3
"""Direct C89->VTK previews, one tile per native MDL skin, never OBJ/TGA.
Usage: python tools/vtk_skin_catalog.py /path/to/model/directory /output/capturas
"""
from pathlib import Path
import argparse
import json
import re
import ctypes as C
from collections import Counter
import vtk
from vtk.util import numpy_support
import numpy as np
from vtk_capture import Reader,vtk_image,capture

class TextureInfo(C.Structure):
    _fields_=[('group',C.c_uint),('slot',C.c_uint),('storage',C.c_uint),('image_type',C.c_uint),
              ('width',C.c_ulong),('height',C.c_ulong),('encoded_bytes',C.c_ulong),
              ('resolved_group',C.c_uint),('resolved_slot',C.c_uint),
              ('has_material',C.c_ubyte),('external',C.c_char*129)]

def collect(reader, model):
    reader.open(model)
    reader.dll.gs3d_vtk_skin_info.argtypes=[C.c_uint,C.c_uint,C.POINTER(TextureInfo)]
    reader.dll.gs3d_vtk_skin_info.restype=C.c_int
    rows=[];all_frames=[];meshes=[]
    gi=0
    while (meta:=reader.info(gi)) is not None:
        nv,nf,nu,ns,frames=meta
        for si in range(ns):
            item=TextureInfo();rc=reader.dll.gs3d_vtk_skin_info(gi,si,C.byref(item))
            arr=reader.skin(gi,si)
            rows.append(dict(model=model.name,group=gi,skin=si,type=int(item.image_type),
                             storage=['embedded','DDS','external','reference','color'][int(item.storage)] if rc==0 else 'error',
                             width=arr.shape[1] if arr is not None else int(item.width),
                             height=arr.shape[0] if arr is not None else int(item.height),
                             bytes=int(item.encoded_bytes),name=item.external.split(b'\0')[0].decode('latin1',errors='replace'),
                             decoded=arr is not None, image=arr))
        group_frames=[]
        for fi in range(frames):
            name=C.create_string_buffer(17)
            if reader.dll.gs3d_vtk_frame_name(gi,fi,name)==0:
                group_frames.append(name.value.decode('latin1',errors='replace').strip())
        all_frames.append(dict(group=gi,count=frames,frames=group_frames))
        meshes.append(dict(group=gi,vertices=nv,faces=nf,skins=ns,frames=frames))
        gi+=1
    reader.dll.gs3d_close()
    return rows,all_frames,meshes

def contact_sheet(rows,dest,width=1600,tile_w=400,tile_h=360):
    n=len(rows);cols=min(4,n);nrows=(n+cols-1)//cols
    win=vtk.vtkRenderWindow();win.SetOffScreenRendering(1);win.SetSize(cols*tile_w,nrows*tile_h);win.SetMultiSamples(0)
    renderers=[]
    for i,r in enumerate(rows):
        col=i%cols;row=i//cols
        ren=vtk.vtkRenderer()
        ren.SetViewport(col/cols,(nrows-row-1)/nrows,(col+1)/cols,(nrows-row)/nrows)
        ren.SetBackground(0.11,0.14,0.20);ren.SetBackground2(0.17,0.21,0.28)
        ren.GradientBackgroundOn();win.AddRenderer(ren)
        data=r['image']
        if data is not None:
            texture=vtk_image(data)
            # textured plane in memory. C89 RGBA -> VTK texture -> framebuffer.
            plane=vtk.vtkPlaneSource();plane.SetOrigin(-1.,-1.,0.);plane.SetPoint1(1.,-1.,0.);plane.SetPoint2(-1.,1.,0.)
            mapper=vtk.vtkPolyDataMapper();mapper.SetInputConnection(plane.GetOutputPort())
            actor=vtk.vtkActor();actor.SetMapper(mapper);actor.SetTexture(texture);actor.GetProperty().SetLighting(False)
            ren.AddActor(actor)
        cam=ren.GetActiveCamera();cam.ParallelProjectionOn();cam.SetPosition(0,0,4);cam.SetFocalPoint(0,0,0);cam.SetViewUp(0,1,0);cam.SetParallelScale(1.36)
        ren.ResetCameraClippingRange()
        label=vtk.vtkTextActor();label.SetInput(f"{r['model']}  |  SKIN {r['skin']}\n{r['storage']}  {r['width']}x{r['height']}  " + ('OK' if data is not None else 'SIN PIXELES'))
        label.GetTextProperty().SetFontSize(15);label.GetTextProperty().SetColor(.96,.97,1.0);label.GetTextProperty().SetBold(True)
        label.SetDisplayPosition(col*tile_w+14,(nrows-row-1)*tile_h+13)
        ren.AddActor2D(label)
        renderers.append((ren,texture if data is not None else None))
    win.Render();snap=vtk.vtkWindowToImageFilter();snap.SetInput(win);snap.ReadFrontBufferOff();snap.SetInputBufferTypeToRGB();snap.Update()
    writer=vtk.vtkPNGWriter();writer.SetInputConnection(snap.GetOutputPort());writer.SetFileName(str(dest));writer.Write();win.Finalize()

def clip_candidates(names):
    groups=[];last=None
    for i,name in enumerate(names):
        base=re.sub(r'\s*\d+\s*$','',name).strip() or name.strip()
        if base!=last:
            groups.append(dict(label=base,first_frame=i,count=1));last=base
        else:groups[-1]['count']+=1
    return groups

def run(input_dir,out):
    out.mkdir(parents=True,exist_ok=True)
    rdr=Reader();all_images=[];report={}
    for model in sorted(Path(input_dir).glob('*.mdl')):
        rows,frames,meshes=collect(rdr,model)
        for r in rows:
            all_images.append(r)
        report[model.name]={"groups":meshes,"skins":[{k:v for k,v in item.items() if k!='image'} for item in rows],
                            "frame_catalog":[{"group":it['group'],"total":it['count'],"frame_names":it['frames'],"candidate_ranges_by_prefix":clip_candidates(it['frames'])} for it in frames]}
        if len(rows):contact_sheet(rows,out/(model.stem+'_ALL_SKINS_VTK.png'))
    if all_images:contact_sheet(all_images,out/'TODAS_LAS_SKINS_VTK.png')
    (out/'modelos_skins_poses_clips.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print('MODELS',len(report),'TOTAL SKINS',len(all_images),'DECODED',sum(x['decoded'] for x in all_images))
    for k,v in report.items():print(k,len(v['skins']),'skins',sum(z['frames'] for z in v['groups']),'frames')

if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('models');ap.add_argument('output');args=ap.parse_args();run(Path(args.models),Path(args.output))
