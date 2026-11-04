#include "hlfp_transparency89.h"
#include <limits.h>
void hlfp_trans89_init(hlfp_trans89_queue *q,hlfp_trans89_triangle *storage,
                       size_t capacity) {
    if (!q) return;
    q->items=storage;q->capacity=storage ? capacity : 0U;q->count=0U;
}
int hlfp_trans89_push(hlfp_trans89_queue *q,const hlfp_mdl_mesh_expanded *mesh,
                       const hlfp_mdl_expanded_triangle *tri) {
    hlfp_trans89_triangle *o;
    int ids[3],j;
    if (!q || !mesh || !tri || !q->items) return HLFP_ERR_BAD_ARGUMENT;
    ids[0]=tri->v0;ids[1]=tri->v1;ids[2]=tri->v2;
    for (j=0;j<3;j++) if (ids[j]<0 || ids[j]>=mesh->num_vertices)
        return HLFP_ERR_OUT_OF_RANGE;
    if (q->count>=q->capacity || q->count>=UINT_MAX) return HLFP_ERR_OVERFLOW;
    o=&q->items[q->count];
    for (j=0;j<3;j++) o->v[j]=mesh->vertices[ids[j]];
    o->texture_index=mesh->texture_index;
    /* Divide first to avoid overflow on 32-bit compilers. */
    o->depth=o->v[0].pos.z/3+o->v[1].pos.z/3+o->v[2].pos.z/3;
    o->serial=(unsigned int)q->count;
    ++q->count;
    return HLFP_OK;
}
static int trans89_greater(const hlfp_trans89_triangle *a,
                           const hlfp_trans89_triangle *b) {
    if (a->depth!=b->depth) return a->depth>b->depth;
    return a->serial>b->serial;
}
static void trans89_swap(hlfp_trans89_triangle *a,hlfp_trans89_triangle *b) {
    hlfp_trans89_triangle tmp=*a;*a=*b;*b=tmp;
}
static void trans89_sift(hlfp_trans89_triangle *a,size_t start,size_t count) {
    size_t root=start;
    for (;;) {
        size_t child;
        if (root>(count-2U)/2U || count<2U) return;
        child=root*2U+1U;
        if (child+1U<count && trans89_greater(&a[child+1U],&a[child])) ++child;
        if (!trans89_greater(&a[child],&a[root])) return;
        trans89_swap(&a[child],&a[root]);root=child;
    }
}
void hlfp_trans89_sort(hlfp_trans89_queue *q) {
    size_t i;
    if (!q || !q->items || q->count<2U) return;
    i=q->count/2U;
    while (i>0U) { --i;trans89_sift(q->items,i,q->count); }
    i=q->count;
    while (i>1U) { --i;trans89_swap(&q->items[0],&q->items[i]);
                    trans89_sift(q->items,0U,i); }
}
