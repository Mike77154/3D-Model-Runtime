#ifndef GOBJ_FIXED_H
#define GOBJ_FIXED_H

#ifdef __cplusplus
extern "C" {
#endif

#define GOBJ_FIXED_SHIFT 16
#define GOBJ_FIXED_ONE   ((long)1 << GOBJ_FIXED_SHIFT)
#define GOBJ_FIXED_HALF  ((long)1 << (GOBJ_FIXED_SHIFT - 1))

typedef long GOBJ_Fixed;

typedef struct GOBJ_Vec2_Tag {
    GOBJ_Fixed u;
    GOBJ_Fixed v;
} GOBJ_Vec2;

typedef struct GOBJ_Vec3_Tag {
    GOBJ_Fixed x;
    GOBJ_Fixed y;
    GOBJ_Fixed z;
} GOBJ_Vec3;

#define GOBJ_FIXED_FROM_INT(x) ((GOBJ_Fixed)((x) << GOBJ_FIXED_SHIFT))
#define GOBJ_FIXED_TO_INT(x)   ((int)((x) >> GOBJ_FIXED_SHIFT))

#ifdef __cplusplus
}
#endif

#endif
