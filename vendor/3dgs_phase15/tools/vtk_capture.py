#!/usr/bin/env python3
"""VTK off-screen screenshot using C89 MDL decoder via ctypes, not OBJ/GLTF conversion.
Only this viewer uses Python/VTK floats: the C89 core remains integer Q16.16.
Build: sh scripts/build_vtk_bridge.sh
Run: python3 tools/vtk_capture.py /path/to/character.mdl /out/capture.png --frame 0
"""
import argparse
import ctypes as C
import math
from pathlib import Path
import sys
import vtk
from vtk.util import numpy_support
import numpy as np

LIB=Path(__file__).resolve().parents[1]/'libgs3d_vtk.so'
class Reader:
    def __init__(self):
        self.dll=C.CDLL(str(LIB))
        u=C.c_uint;ul=C.c_ulong; l=C.c_long; cp=C.POINTER(C.c_ushort)
        self.dll.gs3d_open.argtypes=[C.c_char_p];self.dll.gs3d_open.restype=C.c_int
        self.dll.gs3d_close.argtypes=[]
        self.dll.gs3d_vtk_group.argtypes=[u,C.POINTER(ul),C.POINTER(ul),C.POINTER(ul),C.POINTER(u),C.POINTER(u)]
        self.dll.gs3d_vtk_vertex.argtypes=[u,ul,C.POINTER(l),cp]
        self.dll.gs3d_vtk_uv.argtypes=[u,ul,C.POINTER(l)]
        self.dll.gs3d_vtk_triangle.argtypes=[u,ul,C.POINTER(ul),C.POINTER(ul),C.POINTER(u)]
        self.dll.gs3d_vtk_skin.argtypes=[u,u,C.POINTER(ul),C.POINTER(ul),C.POINTER(C.POINTER(C.c_ubyte))]
        self.dll.gs3d_vtk_audit.argtypes=[u,C.POINTER(ul),C.POINTER(ul),C.POINTER(ul)]
        self.dll.gs3d_apply_deformer.argtypes=[u,u]
        self.dll.gs3d_apply_deformer.restype=C.c_int
        self.dll.gs3d_apply_frame.argtypes=[u,u]
        self.dll.gs3d_apply_frame.restype=C.c_int
        self.dll.gs3d_set_pose_mode.argtypes=[u];self.dll.gs3d_set_pose_mode.restype=C.c_int
        self.dll.gs3d_vtk_frame_name.argtypes=[u,u,C.c_void_p]
        self.dll.gs3d_get_deformer_info.argtypes=[u,C.c_void_p]
        self.dll.gs3d_get_material.argtypes=[u,u,C.c_void_p]
        self._strings=[]

    def open(self,p):
        rc=self.dll.gs3d_open(str(p).encode());
        if rc: raise RuntimeError(f'gs3d_open({p}): {rc}')

    def info(self,gi):
        vals=[C.c_ulong(),C.c_ulong(),C.c_ulong(),C.c_uint(),C.c_uint()]
        rc=self.dll.gs3d_vtk_group(gi,*[C.byref(z) for z in vals]);
        if rc:return None
        return tuple(z.value for z in vals)

    def skin(self,gi,skin):
        w,h=C.c_ulong(),C.c_ulong(); data=C.POINTER(C.c_ubyte)()
        rc=self.dll.gs3d_vtk_skin(gi,skin,C.byref(w),C.byref(h),C.byref(data))
        if rc or not w.value or not h.value:return None
        # each decode overwrites global C89 buffer; immediately copy
        buf=C.string_at(data,int(w.value*h.value*4))
        arr=np.frombuffer(buf,dtype=np.uint8).reshape((int(h.value),int(w.value),4))
        return np.ascontiguousarray(arr)

    def mesh(self,gi,frame=None,animate=False):
        nv,nf,nu,ns,nframes=self.info(gi)
        if frame is not None and nframes:
            if frame>=nframes: frame=nframes-1
            if animate:rc=self.dll.gs3d_apply_deformer(gi,frame)
            else:rc=self.dll.gs3d_apply_frame(gi,frame)
            if rc: print('WARNING pose not applied',gi,frame,'code',rc,file=sys.stderr)
        xyz=(C.c_long*3)(); bone=C.c_ushort();tc=(C.c_long*2)()
        vv=[];uv=[];ff=[];uvf=[];mat=[]
        for i in range(nv):
            if self.dll.gs3d_vtk_vertex(gi,i,xyz,C.byref(bone)):raise ValueError('bad vertex')
            vv.append((xyz[0]/65536,xyz[1]/65536,xyz[2]/65536))
        for i in range(nu):
            if self.dll.gs3d_vtk_uv(gi,i,tc):raise ValueError('bad UV')
            uv.append((tc[0]/65536,tc[1]/65536))
        face=(C.c_ulong*3)();uvface=(C.c_ulong*3)();idx=C.c_uint()
        for i in range(nf):
            if self.dll.gs3d_vtk_triangle(gi,i,face,uvface,C.byref(idx)):raise ValueError('bad face')
            ff.append(tuple(face));uvf.append(tuple(uvface));mat.append(int(idx.value))
        return dict(group=gi, vertices=vv, uvs=uv, faces=ff, face_uvs=uvf, materials=mat, skins=ns, frames=nframes)

def vtk_image(data):
    h,w,_=data.shape
    # MDL RGBA scanlines are top-to-bottom. vtkImageData uses bottom-to-top.
    pix=np.ascontiguousarray(data[::-1,:,:]).reshape(-1,4)
    arr=numpy_support.numpy_to_vtk(pix,deep=True,array_type=vtk.VTK_UNSIGNED_CHAR)
    arr.SetNumberOfComponents(4)
    img=vtk.vtkImageData();img.SetDimensions(w,h,1);img.GetPointData().SetScalars(arr)
    t=vtk.vtkTexture();t.SetInputData(img);t.InterpolateOn();t.RepeatOff();t.EdgeClampOn();return t

def actors(mesh,skins):
    result=[]
    # group by material; positions duplicated at per-corner UV seams
    partitions={}
    for k,m in enumerate(mesh['materials']):partitions.setdefault(m,[]).append(k)
    V=mesh['vertices'];U=mesh['uvs']
    for mat,inds in sorted(partitions.items()):
        points=vtk.vtkPoints();points.SetDataTypeToFloat()
        triangles=vtk.vtkCellArray();tc=vtk.vtkFloatArray();tc.SetNumberOfComponents(2);tc.SetName('uv')
        for face_id in inds:
            verts=mesh['faces'][face_id];uvface=mesh['face_uvs'][face_id]
            tri=vtk.vtkTriangle()
            for j in range(3):
                pos=V[verts[j]];at=points.InsertNextPoint(pos)
                tri.GetPointIds().SetId(j,at)
                tt=U[uvface[j]] if U else (0.,0.)
                tc.InsertNextTuple2(float(tt[0]),float(tt[1]))
            triangles.InsertNextCell(tri)
        pd=vtk.vtkPolyData();pd.SetPoints(points);pd.SetPolys(triangles);pd.GetPointData().SetTCoords(tc)
        norm=vtk.vtkPolyDataNormals();norm.SetInputData(pd);norm.ConsistencyOff();norm.SplittingOff();norm.ComputePointNormalsOn();norm.Update()
        mapper=vtk.vtkPolyDataMapper();mapper.SetInputConnection(norm.GetOutputPort());mapper.ScalarVisibilityOff()
        a=vtk.vtkActor();a.SetMapper(mapper)
        a.GetProperty().SetDiffuse(.55);a.GetProperty().SetAmbient(.55);a.GetProperty().SetSpecular(.17);a.GetProperty().SetSpecularPower(24.)
        a.GetProperty().SetColor(.81,.83,.87)
        if mat in skins:a.SetTexture(skins[mat]);a.GetProperty().SetColor(1.,1.,1.)
        result.append((a,mapper,norm))
    return result

def capture(reader,name,out,frame=0,animate=False,width=1280,height=720,exposure=1.18,force_skin=None):
    chunks=[];minp=np.array([1e20]*3,dtype=float);maxp=-minp.copy();textures=0;counts=[]
    for gi in range(32):
        info=reader.info(gi)
        if info is None:break
        mesh=reader.mesh(gi,frame,animate)
        if not mesh['faces']:continue
        # A few MDL files keep their skins in group 0, shared across other groups.
        skins={}
        for si in range(mesh['skins']):
            data=reader.skin(gi,si)
            if data is not None:
                if exposure!=1.:
                    rgb=data[:,:,:3].astype(np.float32)/255.
                    data=data.copy()
                    data[:,:,:3]=np.clip(np.power(rgb,1.0/exposure)*255.,0.,255.).astype(np.uint8)
                skins[si]=vtk_image(data);textures+=1
        if gi==0: default_skins=skins
        elif not skins:skins=default_skins
        if force_skin is not None and force_skin in skins:
            skins={idx:skins[force_skin] for idx in set(mesh['materials'])}
        points=np.array(mesh['vertices'],dtype=np.float64)
        minp=np.minimum(minp,points.min(axis=0));maxp=np.maximum(maxp,points.max(axis=0))
        chunks.append((mesh,skins));counts.append((mesh['group'],len(mesh['vertices']),len(mesh['faces']),mesh['frames']))
    if not chunks: raise ValueError('No mesh groups')
    center=(minp+maxp)/2;extents=maxp-minp;span=max(1.,float(np.linalg.norm(extents)));maxaxis=float(np.max(extents))
    win=vtk.vtkRenderWindow();win.SetOffScreenRendering(1);win.SetSize(width,height)
    win.SetMultiSamples(0)
    palette=[(0.105,0.13,0.19),(0.13,0.15,0.22)]
    for k,(angle,elev) in enumerate(((0.,13.),(41.,17.))):
        ren=vtk.vtkRenderer();ren.SetViewport(k*.5,0.,(k+1)*.5,1.);ren.SetBackground(*palette[k]);win.AddRenderer(ren)
        for mesh,skins in chunks:
            for a,mapper,norm in actors(mesh,skins):ren.AddActor(a)
        light=vtk.vtkLight();light.SetLightTypeToSceneLight();light.SetPosition(*(center+np.array([span,-span,span])));light.SetFocalPoint(*center);light.SetIntensity(.95);ren.AddLight(light)
        side=vtk.vtkLight();side.SetLightTypeToSceneLight();side.SetPosition(*(center+np.array([-span,span,span*.65])));side.SetFocalPoint(*center);side.SetIntensity(.45);ren.AddLight(side)
        # Z-up; camera initially looks from negative Y.
        az=math.radians(angle);el=math.radians(elev)
        direction=np.array([math.sin(az)*math.cos(el),-math.cos(az)*math.cos(el),math.sin(el)])
        cam=ren.GetActiveCamera();cam.ParallelProjectionOn();cam.SetFocalPoint(*center);cam.SetPosition(*(center+direction*span*2.3));cam.SetViewUp(0.,0.,1.);cam.SetParallelScale(maxaxis*.68 if maxaxis else 1.);ren.ResetCameraClippingRange()
        overlay=vtk.vtkTextActor();overlay.SetInput(('CAMARA A' if k==0 else 'CAMARA B')+f'  |  fotograma {frame}' + ('  |  POSE EXPERIMENTAL - NO VALIDADA' if animate else ''));overlay.GetTextProperty().SetFontSize(20);overlay.GetTextProperty().SetColor(.94,.96,.99);overlay.GetTextProperty().SetBold(True);overlay.SetDisplayPosition(24+(k*width//2),22);ren.AddViewProp(overlay)
    win.Render();filter_=vtk.vtkWindowToImageFilter();filter_.SetInput(win);filter_.SetInputBufferTypeToRGB();filter_.ReadFrontBufferOff();filter_.Update()
    writer=vtk.vtkPNGWriter();writer.SetInputConnection(filter_.GetOutputPort());writer.SetFileName(str(out));writer.Write();win.Finalize()
    return dict(model=name,groups=len(chunks),stats=counts,skins_decoded=textures,range=[minp.tolist(),maxp.tolist()],screenshot=str(out),frame=frame,force_skin=force_skin)

def main():
    ap=argparse.ArgumentParser();ap.add_argument('model');ap.add_argument('output');ap.add_argument('--frame',type=int,default=0);ap.add_argument('--animate',action='store_true',help='Experimental skeletal skinning; validate against Acknex');ap.add_argument('--pose-mode',choices=('file','legacy'),default='file',help='MDL7 animation matrix space (default: file); legacy mode for comparisons')
    ap.add_argument('--exposure',type=float,default=1.18);ap.add_argument('--skin',type=int,default=None,help='For VTK inspection, override face material with skin slot N (not original assignment)');args=ap.parse_args()
    if not LIB.exists():raise SystemExit('Compile shared C89 bridge: sh scripts/build_vtk_bridge.sh')
    reader=Reader();reader.open(args.model);reader.dll.gs3d_set_pose_mode(1 if args.pose_mode=='file' else 0);dest=Path(args.output);dest.parent.mkdir(parents=True,exist_ok=True)
    try:
        info=capture(reader,Path(args.model).stem,dest,args.frame,args.animate,exposure=args.exposure,force_skin=args.skin)
        print(info)
    finally:reader.dll.gs3d_close()
if __name__=='__main__':main()
