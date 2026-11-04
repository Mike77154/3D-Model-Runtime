#!/usr/bin/env python3
"""Render C89-exported multipart OBJ/MTL/TGA as real VTK preview frames.
No image generator. VTK is an optional external diagnostic, not a codec runtime dependency.
Usage: python examples/re4_vtk_multipart.py OUTPUT_DIR FRAME0 FRAME1 ...
Requires the original character textures present in OUTPUT_DIR.
"""
import sys
from pathlib import Path
import vtk
from PIL import Image

def render(path, png):
    ren = vtk.vtkRenderer()
    ren.SetBackground(.085,.10,.14)
    ren.SetBackground2(.19,.22,.26)
    ren.GradientBackgroundOn()
    ren.AutomaticLightCreationOff()
    win = vtk.vtkRenderWindow()
    win.SetOffScreenRendering(True)
    win.SetMultiSamples(0)
    win.SetSize(720,780)
    win.AddRenderer(ren)
    imp=vtk.vtkOBJImporter()
    imp.SetRenderWindow(win)
    imp.SetFileName(str(path))
    imp.SetFileNameMTL(str(path)+'.mtl')
    imp.SetTexturePath(str(path.parent)+'/')
    imp.Update()
    count=ren.GetActors().GetNumberOfItems()
    if count<1:raise RuntimeError('VTK imported no actor for '+str(path))
    b=ren.ComputeVisiblePropBounds()
    cx=(b[0]+b[1])*0.5;cy=(b[2]+b[3])*0.5;cz=(b[4]+b[5])*0.5
    height=b[3]-b[2];span=max(b[1]-b[0],height,b[5]-b[4]);cam=ren.GetActiveCamera()
    cam.ParallelProjectionOn()
    cam.SetViewUp(0,1,0)
    cam.SetFocalPoint(cx,cy,cz)
    cam.SetPosition(cx+span*.20,cy+span*.09,cz+span*2.5)
    cam.SetParallelScale(max(height*.68,span*.72))
    for pos,intensity in [((15,25,30),1.85),((-15,15,12),.9),((0,15,-15),1.0)]:
        light=vtk.vtkLight();light.SetPosition(cx+pos[0],cy+pos[1],cz+pos[2]);light.SetFocalPoint(cx,cy,cz);light.SetIntensity(intensity);ren.AddLight(light)
    ren.ResetCameraClippingRange()
    win.Render()
    snap=vtk.vtkWindowToImageFilter()
    snap.SetInput(win);snap.SetInputBufferTypeToRGBA();snap.ReadFrontBufferOff();snap.Update()
    writer=vtk.vtkPNGWriter();writer.SetFileName(str(png));writer.SetInputConnection(snap.GetOutputPort());writer.Write()
    win.Finalize()
    return count,b

def main():
    if len(sys.argv)<4:raise SystemExit('Usage: re4_vtk_multipart.py OBJ_DIR FRAME0 FRAME1 ...')
    root=Path(sys.argv[1]);frames=[int(s) for s in sys.argv[2:]];imgs=[]
    for frame in frames:
        obj=root/f'frame_{frame:05d}.obj';out=root/f'vtk_{frame:05d}.png'
        actors,b=render(obj,out);print(obj.name,'actors=',actors,'bounds=',b,'=>',out)
        imgs.append(Image.open(out).convert('RGB'))
    if imgs:
        dest=root/'vtk_clip.gif';imgs[0].save(dest,save_all=True,append_images=imgs[1:],duration=175,loop=0)
        print('GIF:',dest)

if __name__=='__main__':main()
