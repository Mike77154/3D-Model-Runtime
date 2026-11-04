#!/usr/bin/env python3
"""Standalone QA for untouched MS3D v4 model meshes.
Not part of the C89 production runtime. Source face counts and manifold
boundary counts distinguish UV/vertex seams from geometric openings.
"""
import argparse,collections,pathlib,struct,json

def inspect(path):
    blob=path.read_bytes()
    if len(blob)<18 or blob[:10]!=b'MS3D000000':raise ValueError('Not a supported MS3D file')
    n=struct.unpack_from('<H',blob,14)[0]
    pos=[struct.unpack_from('<3f',blob,16+i*15+1) for i in range(n)]
    o=16+n*15;nt=struct.unpack_from('<H',blob,o)[0];o+=2
    index_edges=collections.Counter();geometric_edges=collections.Counter();flags=collections.Counter();duplicates=collections.Counter()
    for i in range(nt):
        flag,a,b,c=struct.unpack_from('<4H',blob,o);o+=70
        flags[flag]+=1
        duplicates[tuple(sorted((a,b,c)))]+=1
        for x,y in ((a,b),(b,c),(c,a)):
            index_edges[tuple(sorted((x,y)))]+=1
            geometric_edges[tuple(sorted((pos[x],pos[y])))]+=1
    return {'model':path.name,'vertices':n,'triangles':nt,
            'boundary_edges_index':sum(v==1 for v in index_edges.values()),
            'boundary_edges_geometric':sum(v==1 for v in geometric_edges.values()),
            'nonmanifold_edges_geometric':sum(v>2 for v in geometric_edges.values()),
            'duplicate_triangles_by_indices':sum(v>1 for v in duplicates.values()),
            'hidden_flagged_triangles':sum(c for flag,c in flags.items() if flag&2),
            'meaning':'Boundaries do not necessarily imply missing polygons.'}

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('model',nargs='+');p.add_argument('--json',action='store_true');a=p.parse_args()
    for name in a.model:
        result=inspect(pathlib.Path(name))
        if a.json:print(json.dumps(result,ensure_ascii=False))
        else:print(result)
