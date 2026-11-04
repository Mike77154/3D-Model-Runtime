#include "lws3d.h"
#ifndef LWS3D_NO_STDIO
#include <stdio.h>

static void print_fx(const char *label, LWS3D_Fixed v)
{
    char b[40];
    lws3d_fixed_to_string(v, b, (int)sizeof(b), 4);
    printf("%s%s", label, b);
}

static void print_vec3(const LWS3D_Vec3 *v)
{
    char a[32];
    char b[32];
    char c[32];
    lws3d_fixed_to_string(v->x, a, (int)sizeof(a), 4);
    lws3d_fixed_to_string(v->y, b, (int)sizeof(b), 4);
    lws3d_fixed_to_string(v->z, c, (int)sizeof(c), 4);
    printf("%s %s %s", a, b, c);
}

void lws3d_dump_scene_stdout(const LWS3D_Scene *s)
{
    int i;
    if (!s) return;
    printf("LWSC %d frames %ld..%ld step %ld ", s->lwsc_version, s->first_frame, s->last_frame, s->frame_step);
    print_fx("fps ", s->frames_per_second);
    printf("\n");
    printf("items=%d lights=%d cameras=%d unknown_keywords=%d\n", s->item_count, s->light_count, s->camera_count, s->unknown_count);
    for (i = 0; i < s->item_count; ++i) {
        const LWS3D_Item *it;
        it = &s->items[i];
        printf("item[%d] kind=%s layer=%d name='%s' path='%s' parent=%s:%ld keys0=%d\n",
               i, lws3d_item_kind_name(it->kind), it->layer, it->name, it->path,
               lws3d_parent_type_name(it->parent_type), it->parent_index, it->motion.channel[0].key_count);
    }
    for (i = 0; i < s->light_count; ++i) {
        const LWS3D_Light *li;
        li = &s->lights[i];
        printf("light[%d] name='%s' type=%d color=", i, li->name, li->light_type);
        print_vec3(&li->color);
        print_fx(" intensity=", li->intensity);
        printf("\n");
    }
    for (i = 0; i < s->camera_count; ++i) {
        const LWS3D_Camera *ca;
        ca = &s->cameras[i];
        printf("camera[%d] name='%s' ", i, ca->name);
        print_fx("zoom=", ca->zoom_factor);
        printf("\n");
    }
}
#endif
