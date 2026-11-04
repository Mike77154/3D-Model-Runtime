/* Source 2013 studio_virtualmodel.cpp AppendModels: recursive include graph.
   Explicit caller-owned MDLs, no filesystem, no heap, strictly ISO C89.
   Source does not guarantee that all includes are reachable; this API
   intentionally fails closed for absent, orphaned or cyclic dependencies. */
#include "hl2modelib.h"
#include <string.h>

/* The v123 vendor performs the unchanged catalog assembly after this vendor
   has validated the graph and produced its Source-style DFS ordering. */
int hl2m_virtual_build_ordered124_internal(const hl2m_virtual_input123 *,
    unsigned int,hl2m_virtual_multi123 *);

static int v124_lower(int x){return x>='A'&&x<='Z'?x+'a'-'A':x;}
static int v124_equal(const char *a,const char *b)
{
    unsigned int i;int x,y;
    if(!a||!b)return 0;
    for(i=0u;i<HL2M_PATH_MAX;i++){
        x=(unsigned char)a[i];y=(unsigned char)b[i];
        if(x=='\\')x='/';
        if(y=='\\')y='/';
        if(v124_lower(x)!=v124_lower(y))return 0;
        if(!x)return 1;
    }
    return 0;
}
typedef struct v124_context_s {
    const hl2m_virtual_input123 *source;
    unsigned int count,used;
    int state[HL2M_VIRTUAL_GROUP_CAP123]; /* 0 = new, 1 = active, 2 = done */
    int parent[HL2M_VIRTUAL_GROUP_CAP123];
    int order[HL2M_VIRTUAL_GROUP_CAP123];
} v124_context;
static int v124_find(const v124_context *c,const char *path)
{
    unsigned int i;
    for(i=1u;i<c->count;i++)
        if(v124_equal(path,c->source[i].include_path))return (int)i;
    return -1;
}
static int v124_visit(v124_context *c,int node,int from)
{
    char names[HL2M_VIRTUAL_INCLUDE_CAP122][HL2M_PATH_MAX];
    unsigned int n=0u,j;int child,r;
    const hl2m_virtual_input123 *src=&c->source[node];
    if(c->state[node]==1)return HL2M_ERR_PARSE; /* cycle, including self */
    if(c->state[node]==2)return HL2M_OK; /* diamond/shared bank: once */
    if(c->used>=HL2M_VIRTUAL_GROUP_CAP123)return HL2M_ERR_LIMIT;
    c->state[node]=1;c->parent[node]=from;c->order[c->used++]=node;
    r=hl2m_virtual_includes122(src->mdl_bytes,src->mdl_size,names,
        HL2M_VIRTUAL_INCLUDE_CAP122,&n);
    if(r)return r;
    for(j=0u;j<n;j++){
        /* A child referencing the base model is an actual cycle. */
        if(c->source[0].include_path &&
           v124_equal(names[j],c->source[0].include_path))
           return HL2M_ERR_PARSE;
        child=v124_find(c,names[j]);
        if(child<0)return HL2M_ERR_RANGE;
        r=v124_visit(c,child,node);if(r)return r;
    }
    c->state[node]=2;
    return HL2M_OK;
}
int hl2m_virtual_build_recursive124(const hl2m_virtual_input123 *sources,
    unsigned int count,hl2m_virtual_multi123 *out,
    hl2m_virtual_graph124 *graph)
{
    v124_context ctx;hl2m_virtual_input123 ordered[HL2M_VIRTUAL_GROUP_CAP123];
    hl2m_virtual_multi123 result;hl2m_virtual_graph124 g;
    unsigned int i,j;int r;
    if(!sources||!out||!graph)return HL2M_ERR_NULL;
    if(count==0u||count>HL2M_VIRTUAL_GROUP_CAP123)return HL2M_ERR_LIMIT;
    memset(&ctx,0,sizeof(ctx));ctx.source=sources;ctx.count=count;
    for(i=0u;i<count;i++){
        if(!sources[i].mdl||!sources[i].mdl_bytes)return HL2M_ERR_NULL;
        if(i>0u&&(!sources[i].include_path||!sources[i].include_path[0]))
            return HL2M_ERR_NULL;
        for(j=1u;j<i;j++)if(v124_equal(sources[i].include_path,
            sources[j].include_path))return HL2M_ERR_PARSE;
    }
    for(i=0u;i<HL2M_VIRTUAL_GROUP_CAP123;i++)ctx.parent[i]=-1;
    r=v124_visit(&ctx,0,-1);if(r)return r;
    if(ctx.used!=count)return HL2M_ERR_RANGE; /* supplied orphan */
    memset(&g,0,sizeof(g));g.group_count=(int)count;
    for(i=0u;i<count;i++){
        int original=ctx.order[i];
        ordered[i]=sources[original];
        g.group_to_input[i]=original;
        g.input_to_group[original]=(int)i;
    }
    for(i=0u;i<count;i++){
        int original=ctx.order[i];
        int parent=ctx.parent[original];
        g.parent_group[i]=parent<0?-1:g.input_to_group[parent];
    }
    r=hl2m_virtual_build_ordered124_internal(ordered,count,&result);
    if(r)return r;
    *out=result;*graph=g;return HL2M_OK;
}
/* Source CStudioHdr::iRelativeSeq(): local group sequence IDs are mapped to
   the virtual catalog. This also resolves forward-declared/OVERRIDE entries
   whose actual owner may be in another included model group. */
int hl2m_virtual_layer_target124(const hl2m_virtual_multi123 *catalog,
    int parent_virtual_seq,int layer_local_seq,int *target_virtual_seq)
{
    hl2m_virtual_ref122 ref;const hl2m_virtual_group123 *g;
    int id;
    if(!catalog||!target_virtual_seq)return HL2M_ERR_NULL;
    if(parent_virtual_seq<0||parent_virtual_seq>=catalog->seq_count)
        return HL2M_ERR_RANGE;
    ref=catalog->sequences[parent_virtual_seq];
    if(ref.group<0||ref.group>=catalog->group_count)return HL2M_ERR_RANGE;
    g=&catalog->groups[ref.group];
    if(layer_local_seq<0||layer_local_seq>=g->source.mdl->sequence_count)
        return HL2M_ERR_RANGE;
    id=g->seq_to_virtual[layer_local_seq];
    if(id<0||id>=catalog->seq_count)return HL2M_ERR_RANGE;
    *target_virtual_seq=id;return HL2M_OK;
}
