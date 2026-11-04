#include "gltf3d_util.h"
#include <string.h>

static int in_range_i(int v,int n){ return v>=0 && v<n; }

int gltf3d_component_size(int component_type)
{
    if(component_type==GLTF3D_COMPONENT_BYTE || component_type==GLTF3D_COMPONENT_UBYTE) return 1;
    if(component_type==GLTF3D_COMPONENT_SHORT || component_type==GLTF3D_COMPONENT_USHORT) return 2;
    if(component_type==GLTF3D_COMPONENT_UINT || component_type==GLTF3D_COMPONENT_FLOAT) return 4;
    return 0;
}

unsigned long gltf3d_accessor_elem_size(const gltf3d_accessor *ac)
{
    int cs;
    if(!ac) return 0;
    cs=gltf3d_component_size(ac->component_type);
    if(cs<=0 || ac->elem_count<=0) return 0;
    return (unsigned long)cs*(unsigned long)ac->elem_count;
}

int gltf3d_accessor_byte_range(const gltf3d_asset *a, int accessor_index,
                               unsigned long *byte_offset, unsigned long *byte_length,
                               unsigned long *stride)
{
    const gltf3d_accessor *ac;
    const gltf3d_bufferview *bv;
    unsigned long esz, st, last;
    if(!a || accessor_index<0 || accessor_index>=a->accessor_count) return GLTF3D_ERR_BAD_ARG;
    ac=&a->accessors[accessor_index];
    if(ac->buffer_view<0) return GLTF3D_ERR_UNSUPPORTED;
    if(!in_range_i(ac->buffer_view,a->buffer_view_count)) return GLTF3D_ERR_RANGE;
    bv=&a->buffer_views[ac->buffer_view];
    esz=gltf3d_accessor_elem_size(ac);
    if(esz==0) return GLTF3D_ERR_UNSUPPORTED;
    st=bv->byte_stride?bv->byte_stride:esz;
    if(st<esz) return GLTF3D_ERR_RANGE;
    if(ac->count==0) return GLTF3D_ERR_RANGE;
    last=ac->byte_offset+(ac->count-1UL)*st+esz;
    if(last>bv->byte_length) return GLTF3D_ERR_RANGE;
    if(byte_offset) *byte_offset=bv->byte_offset+ac->byte_offset;
    if(byte_length) *byte_length=last-ac->byte_offset;
    if(stride) *stride=st;
    return GLTF3D_OK;
}

int gltf3d_accessor_compute_bounds(const gltf3d_asset *a, int accessor_index,
                                   gltf3d_fx *minv, gltf3d_fx *maxv, int max_comp)
{
    const gltf3d_accessor *ac;
    unsigned long i;
    int c,r,limit;
    gltf3d_fx v;
    if(!a || !minv || !maxv || accessor_index<0 || accessor_index>=a->accessor_count) return GLTF3D_ERR_BAD_ARG;
    ac=&a->accessors[accessor_index];
    limit=ac->elem_count;
    if(limit>max_comp) limit=max_comp;
    if(limit<=0 || ac->count==0) return GLTF3D_ERR_RANGE;
    for(c=0;c<limit;c++){ minv[c]=0x7fffffffL; maxv[c]=(-0x7fffffffL)-1L; }
    for(i=0;i<ac->count;i++){
        for(c=0;c<limit;c++){
            r=gltf3d_accessor_read_fixed(a,accessor_index,i,c,&v);
            if(r<0) return r;
            if(v<minv[c]) minv[c]=v;
            if(v>maxv[c]) maxv[c]=v;
        }
    }
    return GLTF3D_OK;
}

unsigned long gltf3d_primitive_vertex_count(const gltf3d_asset *a, int prim_index)
{
    int ai;
    if(!a || prim_index<0 || prim_index>=a->primitive_count) return 0;
    ai=a->primitives[prim_index].attributes[GLTF3D_ATTR_POSITION];
    if(ai<0 || ai>=a->accessor_count) return 0;
    return a->accessors[ai].count;
}

unsigned long gltf3d_primitive_index_count(const gltf3d_asset *a, int prim_index)
{
    int ai;
    if(!a || prim_index<0 || prim_index>=a->primitive_count) return 0;
    ai=a->primitives[prim_index].indices;
    if(ai>=0 && ai<a->accessor_count) return a->accessors[ai].count;
    return gltf3d_primitive_vertex_count(a,prim_index);
}

unsigned long gltf3d_primitive_triangle_count(const gltf3d_asset *a, int prim_index)
{
    unsigned long n;
    int mode;
    if(!a || prim_index<0 || prim_index>=a->primitive_count) return 0;
    mode=a->primitives[prim_index].mode;
    n=gltf3d_primitive_index_count(a,prim_index);
    if(mode==GLTF3D_MODE_TRIANGLES) return n/3UL;
    if(mode==GLTF3D_MODE_TRIANGLE_STRIP || mode==GLTF3D_MODE_TRIANGLE_FAN) return n>=3UL ? n-2UL : 0UL;
    return 0;
}

static int read_prim_index(const gltf3d_asset *a, int prim_index, unsigned long element, unsigned long *out)
{
    int ai;
    if(!a || !out || prim_index<0 || prim_index>=a->primitive_count) return GLTF3D_ERR_BAD_ARG;
    ai=a->primitives[prim_index].indices;
    if(ai>=0) return gltf3d_accessor_read_u32(a,ai,element,out);
    *out=element;
    return GLTF3D_OK;
}

int gltf3d_primitive_read_triangle(const gltf3d_asset *a, int prim_index,
                                   unsigned long tri_index,
                                   unsigned long out_indices[3])
{
    int mode,r;
    unsigned long n,base,tmp;
    if(!a || !out_indices || prim_index<0 || prim_index>=a->primitive_count) return GLTF3D_ERR_BAD_ARG;
    mode=a->primitives[prim_index].mode;
    n=gltf3d_primitive_triangle_count(a,prim_index);
    if(tri_index>=n) return GLTF3D_ERR_RANGE;
    if(mode==GLTF3D_MODE_TRIANGLES){
        base=tri_index*3UL;
        r=read_prim_index(a,prim_index,base,&out_indices[0]); if(r<0) return r;
        r=read_prim_index(a,prim_index,base+1UL,&out_indices[1]); if(r<0) return r;
        r=read_prim_index(a,prim_index,base+2UL,&out_indices[2]); if(r<0) return r;
        return GLTF3D_OK;
    }
    if(mode==GLTF3D_MODE_TRIANGLE_STRIP){
        r=read_prim_index(a,prim_index,tri_index,&out_indices[0]); if(r<0) return r;
        r=read_prim_index(a,prim_index,tri_index+1UL,&out_indices[1]); if(r<0) return r;
        r=read_prim_index(a,prim_index,tri_index+2UL,&out_indices[2]); if(r<0) return r;
        if(tri_index&1UL){ tmp=out_indices[0]; out_indices[0]=out_indices[1]; out_indices[1]=tmp; }
        return GLTF3D_OK;
    }
    if(mode==GLTF3D_MODE_TRIANGLE_FAN){
        r=read_prim_index(a,prim_index,0,&out_indices[0]); if(r<0) return r;
        r=read_prim_index(a,prim_index,tri_index+1UL,&out_indices[1]); if(r<0) return r;
        r=read_prim_index(a,prim_index,tri_index+2UL,&out_indices[2]); if(r<0) return r;
        return GLTF3D_OK;
    }
    return GLTF3D_ERR_UNSUPPORTED;
}

static void set_identity_at(gltf3d_fx *mats,int node)
{
    gltf3d_fx *m;
    m=mats+((unsigned long)node*16UL);
    gltf3d_fx_mat4_identity(m);
}

static int compute_node_world(const gltf3d_asset *a,int node,const gltf3d_fx parent[16],gltf3d_fx *mats,int cap,unsigned char *mark)
{
    gltf3d_fx local[16],world[16];
    int i,ch;
    if(node<0 || node>=a->node_count || node>=cap) return GLTF3D_ERR_RANGE;
    gltf3d_node_local_matrix(&a->nodes[node],local);
    gltf3d_fx_mat4_mul(world,parent,local);
    for(i=0;i<16;i++) mats[(unsigned long)node*16UL+(unsigned long)i]=world[i];
    mark[node]=1U;
    for(i=0;i<a->nodes[node].child_count;i++){
        ch=a->nodes[node].children[i];
        if(ch==node) return GLTF3D_ERR_RANGE;
        if(ch>=0 && ch<a->node_count){
            int r=compute_node_world(a,ch,world,mats,cap,mark);
            if(r<0) return r;
        }
    }
    return GLTF3D_OK;
}

int gltf3d_node_world_matrices(const gltf3d_asset *a, int scene_index,
                               gltf3d_fx *out_matrices16,
                               int out_node_capacity)
{
    gltf3d_fx id[16];
    unsigned char mark[GLTF3D_MAX_NODES];
    int i,node,r,si;
    if(!a || !out_matrices16 || out_node_capacity<a->node_count) return GLTF3D_ERR_BAD_ARG;
    for(i=0;i<a->node_count;i++){ set_identity_at(out_matrices16,i); mark[i]=0U; }
    si=scene_index;
    if(si<0) si=a->scene;
    if(si<0 && a->scene_count>0) si=0;
    if(si<0 || si>=a->scene_count) return GLTF3D_ERR_RANGE;
    gltf3d_fx_mat4_identity(id);
    for(i=0;i<a->scenes[si].node_count;i++){
        node=a->scenes[si].nodes[i];
        r=compute_node_world(a,node,id,out_matrices16,out_node_capacity,mark);
        if(r<0) return r;
    }
    return GLTF3D_OK;
}

int gltf3d_transform_point(const gltf3d_fx m[16],
                           gltf3d_fx x, gltf3d_fx y, gltf3d_fx z,
                           gltf3d_fx out_xyz[3])
{
    if(!m || !out_xyz) return GLTF3D_ERR_BAD_ARG;
    out_xyz[0]=gltf3d_fx_mul(x,m[0])+gltf3d_fx_mul(y,m[4])+gltf3d_fx_mul(z,m[8])+m[12];
    out_xyz[1]=gltf3d_fx_mul(x,m[1])+gltf3d_fx_mul(y,m[5])+gltf3d_fx_mul(z,m[9])+m[13];
    out_xyz[2]=gltf3d_fx_mul(x,m[2])+gltf3d_fx_mul(y,m[6])+gltf3d_fx_mul(z,m[10])+m[14];
    return GLTF3D_OK;
}
