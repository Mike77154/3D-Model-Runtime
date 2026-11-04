#!/usr/bin/env python3
"""Read real MDL files directly through the bounded C89 bridge; produces JSON only."""
import ctypes as C
import json
from pathlib import Path
from vtk_capture import Reader

def scan(folder):
    reader=Reader();result=[]
    for f in sorted(Path(folder).glob('*.mdl')):
        reader.open(f);gr=[]
        try:
            for g in range(32):
                info=reader.info(g)
                if info is None:break
                a,b,c=C.c_ulong(),C.c_ulong(),C.c_ulong()
                r=reader.dll.gs3d_vtk_audit(g,C.byref(a),C.byref(b),C.byref(c))
                if r: raise RuntimeError(f'audit fail {f.name} {g} {r}')
                gr.append(dict(group=g,vertices=info[0],triangles=info[1],uvs=info[2],skins=info[3],frames=info[4],degenerate_faces=a.value,weighted_vertices=b.value,weight_influences=c.value))
            result.append(dict(model=f.name,bytes=f.stat().st_size,groups=gr))
        finally:reader.dll.gs3d_close()
    return result
if __name__=='__main__':
    root=Path(__file__).resolve().parents[1]
    out=scan(root/'tests/real_corpus_local')
    fn=root/'capturas_vtk'/'diagnostico_corpus_vtk.json'
    fn.write_text(json.dumps(out,indent=2,ensure_ascii=False))
    print(json.dumps(out,indent=2,ensure_ascii=False))
