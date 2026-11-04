#ifndef HLFP_TRANSPARENCY89_H
#define HLFP_TRANSPARENCY89_H
#include "hlfp.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct hlfp_trans89_triangle_s {
    hlfp_mdl_expanded_vertex v[3];
    int texture_index;
    hlfp_i32 depth;
    unsigned int serial;
} hlfp_trans89_triangle;
typedef struct hlfp_trans89_queue_s {
    hlfp_trans89_triangle *items; /* storage supplied by caller */
    size_t capacity;
    size_t count;
} hlfp_trans89_queue;
void hlfp_trans89_init(hlfp_trans89_queue *q, hlfp_trans89_triangle *storage,
                        size_t capacity);
int hlfp_trans89_push(hlfp_trans89_queue *q, const hlfp_mdl_mesh_expanded *mesh,
                        const hlfp_mdl_expanded_triangle *tri);
void hlfp_trans89_sort(hlfp_trans89_queue *q);
#ifdef __cplusplus
}
#endif
#endif
