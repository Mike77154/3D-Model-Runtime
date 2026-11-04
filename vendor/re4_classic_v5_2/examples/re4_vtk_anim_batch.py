#!/usr/bin/env python3
"""Diagnostic offline RE4 2007 FCV -> C89 skeletal pose -> textured GLB -> VTK PNG/GIF.

Not part of C89 library. Requires Python trimesh, VTK and Pillow.
Original game assets must be supplied by user; none are bundled.
Usage:
 python examples/re4_vtk_anim_batch.py --c-tool ./re4_pmd_pose \
   --pmd model.pmd --fcv clip.fcv --out ./renders --frames 0,3,6,9,12
Any extra pieces need their own PMD + matching animation/attachment transform.
"""
import argparse
import subprocess
from pathlib import Path
import trimesh
import vtk
from PIL import Image

def save_snapshot(glb,output,width=660,height=780):
    renderer=vtk.vtkRenderer()
    renderer.AutomaticLightCreationOff()
    renderer.GradientBackgroundOn()
    renderer.SetBackground(.075,.1,.145)
    renderer.SetBackground2(.16,.2,.245)
    window=vtk.vtkRenderWindow()
    window.SetOffScreenRendering(1)
    window.SetSize(width,height)
    window.SetMultiSamples(0)
    window.AddRenderer(renderer)
    importer=vtk.vtkGLTFImporter()
    importer.SetFileName(str(glb))
    importer.SetRenderWindow(window)
    importer.Update()
    if renderer.GetActors().GetNumberOfItems()==0:
        raise RuntimeError('VTK did not load geometry')
    for pos,power in [((17,25,35),2.1),((-25,17,12),1.25),((0,24,-25),1.55),((2,7,28),.55)]:
        light=vtk.vtkLight()
        light.SetPosition(*pos)
        light.SetFocalPoint(0,9,0)
        light.SetIntensity(power)
        renderer.AddLight(light)
    cam=renderer.GetActiveCamera()
    cam.ParallelProjectionOn()
    cam.SetViewUp(0,1,0)
    cam.SetFocalPoint(0,9,0)
    cam.SetPosition(0,11,40)
    cam.SetParallelScale(11.5)
    renderer.ResetCameraClippingRange()
    window.Render()
    capture=vtk.vtkWindowToImageFilter()
    capture.SetInput(window)
    capture.SetInputBufferTypeToRGBA()
    capture.ReadFrontBufferOff()
    capture.Update()
    writer=vtk.vtkPNGWriter()
    writer.SetFileName(str(output))
    writer.SetInputConnection(capture.GetOutputPort())
    writer.Write()
    window.Finalize()

def batch(c_tool,pmd,fcv,out,frames):
    out.mkdir(exist_ok=True,parents=True)
    files=[]
    for frame in frames:
        obj=out/f'frame_{frame:04d}.obj'
        subprocess.run([str(c_tool.resolve()),str(pmd.resolve()),str(fcv.resolve()),str(frame),str(obj)],check=True)
        glb=out/f'frame_{frame:04d}.glb'
        trimesh.load(str(obj),force='scene',process=False).export(str(glb))
        png=out/f'frame_{frame:04d}_vtk.png'
        save_snapshot(glb,png)
        files.append(png)
    imgs=[Image.open(x).convert('RGB') for x in files]
    if imgs:
        imgs[0].save(out/'re4_fcv_vtk.gif',save_all=True,append_images=imgs[1:],duration=120,loop=0,optimize=False)
    return files

def main():
    a=argparse.ArgumentParser(description=__doc__)
    a.add_argument('--c-tool',type=Path,required=True)
    a.add_argument('--pmd',type=Path,required=True)
    a.add_argument('--fcv',type=Path,required=True)
    a.add_argument('--out',type=Path,required=True)
    a.add_argument('--frames',default='0,3,6,9,12')
    o=a.parse_args()
    batch(o.c_tool,o.pmd,o.fcv,o.out,[int(x) for x in o.frames.split(',')])
if __name__=='__main__':main()
