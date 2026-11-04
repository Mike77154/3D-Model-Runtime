#!/usr/bin/env python3
"""Diagnostic stills from real PMD/FCV via native C89 -> OBJ -> VTK.
Requires VTK only for screenshots. Needs user-owned game archives at runtime.
No synthetic mesh is generated.
"""
import argparse,csv,subprocess
from pathlib import Path
from re4_all_characters import indexes,parse_dat,read_at


def render_obj(src,dest):
    import vtk
    rdr=vtk.vtkOBJReader();rdr.SetFileName(str(src));rdr.Update()
    poly=rdr.GetOutput()
    if poly.GetNumberOfPoints()<3 or poly.GetNumberOfCells()<1:
        raise RuntimeError('no geometry in native OBJ')
    normals=vtk.vtkPolyDataNormals();normals.SetInputData(poly);normals.SplittingOff()
    normals.ConsistencyOn();normals.AutoOrientNormalsOff();normals.Update()
    mapper=vtk.vtkPolyDataMapper();mapper.SetInputConnection(normals.GetOutputPort())
    actor=vtk.vtkActor();actor.SetMapper(mapper)
    actor.GetProperty().SetColor(0.76,0.85,0.91)
    actor.GetProperty().SetAmbient(0.28)
    actor.GetProperty().SetDiffuse(0.63)
    actor.GetProperty().SetSpecular(0.17)
    actor.GetProperty().SetSpecularPower(18)
    ren=vtk.vtkRenderer();ren.SetBackground(0.07,0.09,0.12);ren.AddActor(actor)
    wnd=vtk.vtkRenderWindow();wnd.SetOffScreenRendering(1);wnd.SetSize(640,720);wnd.AddRenderer(ren)
    b=poly.GetBounds()
    ctr=((b[0]+b[1])/2,(b[2]+b[3])/2,(b[4]+b[5])/2)
    extent=max(b[1]-b[0],b[3]-b[2],b[5]-b[4],1.0)
    cam=ren.GetActiveCamera()
    # RE4 PMD 2007 uses Y as upright axis in diagnostic OBJ export.
    cam.SetFocalPoint(*ctr)
    cam.SetPosition(ctr[0]+extent*1.05,ctr[1]+extent*0.15,ctr[2]+extent*1.9)
    cam.SetViewUp(0,1,0)
    cam.ParallelProjectionOn();cam.SetParallelScale(extent*0.82)
    ren.ResetCameraClippingRange()
    wnd.Render()
    capture=vtk.vtkWindowToImageFilter();capture.SetInput(wnd);capture.Update()
    png=vtk.vtkPNGWriter();png.SetFileName(str(dest));png.SetInputConnection(capture.GetOutputPort());png.Write()
    wnd.Finalize()
    return poly.GetNumberOfPoints(),poly.GetNumberOfCells()


def run(args):
    base=Path(__file__).resolve().parent.parent
    rows={r['family']:r for r in csv.DictReader(open(args.family_csv,encoding='utf-8'))}
    xidx={e['name'].lower():e for e in indexes(base/'re4_gca_catalog',args.xfile)}
    emidx={e['name'].lower():e for e in indexes(base/'re4_gca_catalog',args.em)}
    out=Path(args.out).resolve();out.mkdir(parents=True,exist_ok=True)
    report=[]
    with open(args.xfile,'rb') as xf,open(args.em,'rb') as ef:
        for family in args.families.split(','):
            meta=rows.get(family)
            if not meta or not meta['base_pmd'] or not meta['animation_dat'] or not meta['sample_fcv_index']:
                print(family,': no mapped sample');continue
            fdir=out/family;fdir.mkdir(exist_ok=True)
            pmd=read_at(xf,xidx[meta['base_pmd'].lower()])
            dat=read_at(ef,emidx[meta['animation_dat'].lower()],cap=32*1024*1024)
            candidate=int(meta['sample_fcv_index'])
            cliprow=next((row for row in parse_dat(dat) if row[0]==candidate and row[1]=='FCV'),None)
            if not cliprow:
                print(family,': bad FCV index');continue
            idx,ext,start,length=cliprow
            pmdpath=fdir/'model.pmd';fcvpath=fdir/'motion.fcv'
            pmdpath.write_bytes(pmd);fcvpath.write_bytes(dat[start:start+length])
            frames=(0,6)
            for frame in frames:
                outobj=fdir/f'{family}_f{frame:03}.obj'
                cmd=[str(base/'re4_pmd_pose'),str(pmdpath),str(fcvpath),str(frame),str(outobj)]
                done=subprocess.run(cmd,text=True,capture_output=True)
                if done.returncode:
                    print(family,frame,'OBJ error',done.stderr[:300]);continue
                img=fdir/f'{family}_f{frame:03}_VTK.png'
                count=render_obj(outobj,img)
                report.append({'family':family,'frame':frame,'fcv_dat_index':candidate,'vtk_points':count[0],
                               'vtk_cells':count[1],'png':img.name})
                print('rendered',family,frame,count)
            # Keep only screenshots in the evidence bundle. No proprietary PMD/FCV/OBJ.
            if not args.keep_originals:
                for e in fdir.iterdir():
                    if e.suffix.lower() in ('.pmd','.fcv','.obj','.mtl'):
                        e.unlink()
    with open(out/'render_validation.csv','w',newline='',encoding='utf-8') as f:
        w=csv.DictWriter(f,fieldnames=['family','frame','fcv_dat_index','vtk_points','vtk_cells','png'])
        w.writeheader();w.writerows(report)

if __name__=='__main__':
    a=argparse.ArgumentParser()
    a.add_argument('--xfile',required=True);a.add_argument('--em',required=True)
    a.add_argument('--family-csv',required=True);a.add_argument('--out',required=True)
    a.add_argument('--families',default='em11,em2c,pl05')
    a.add_argument('--keep-originals',action='store_true')
    run(a.parse_args())
