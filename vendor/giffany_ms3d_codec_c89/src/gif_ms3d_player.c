#include "gif_ms3d_player.h"
#include <string.h>

static unsigned long fx_milliseconds(ms3d_fx t)
{
    unsigned long seconds,frac;
    if(t<=0L)return 0UL;
    seconds=(unsigned long)(t/MS3D_FX_ONE);
    frac=(unsigned long)(t%MS3D_FX_ONE);
    return seconds*1000UL + ((frac*1000UL+32768UL)/65536UL);
}
static ms3d_fx milliseconds_fx(unsigned long ms)
{
    /* Long multiplication stays 32-bit safe when seconds <= 32767. */
    return (ms3d_fx)((ms/1000UL)*65536UL +
                     (((ms%1000UL)*65536UL+500UL)/1000UL));
}
unsigned long ms3d_player_model_duration_ms(const ms3d_model *m)
{
    unsigned i;
    ms3d_fx end=0L,frames;
    if(!m)return 0UL;
    if(m->animation_fps>0L && m->total_frames>0L && m->total_frames<32767L){
        frames=ms3d_fx_div(ms3d_fx_from_int(m->total_frames),m->animation_fps);
        if(frames>end)end=frames;
    }
    for(i=0U;i<m->num_rot_keys;++i)
        if(m->rot_keys[i].time>end)end=m->rot_keys[i].time;
    for(i=0U;i<m->num_pos_keys;++i)
        if(m->pos_keys[i].time>end)end=m->pos_keys[i].time;
    return fx_milliseconds(end);
}
int ms3d_player_prepare(ms3d_player *p,const ms3d_model *m,
                        ms3d_skin_pose *pose,ms3d_skin_vertices *verts,
                        ms3d_corner_normals *normals,
                        ms3d_uv_bindings *bindings,int mode)
{
    int rc;
    if(!p||!m||!pose||!verts||!normals||!bindings)return MS3D_E_NULL;
    rc=ms3d_validate(m);if(rc!=MS3D_OK)return rc;
    rc=ms3d_skin_prepare(m,pose);if(rc!=MS3D_OK)return rc;
    rc=ms3d_build_corner_normals(m,normals,mode);if(rc!=MS3D_OK)return rc;
    rc=ms3d_uv_build_bindings(m,bindings);if(rc!=MS3D_OK)return rc;
    memset(p,0,sizeof(*p));
    p->model=m;p->pose=pose;p->vertices=verts;
    p->normals=normals;p->bindings=bindings;
    p->duration_ms=ms3d_player_model_duration_ms(m);
    if(p->duration_ms>32767000UL)return MS3D_E_OVERFLOW;
    p->end_ms=p->duration_ms;
    p->playback_mode=MS3D_PLAYER_LOOP;
    p->rotation_mode=MS3D_ROTATION_QUAT_NLERP;
    return MS3D_OK;
}
int ms3d_player_range(ms3d_player *p,unsigned long first,unsigned long last,int mode)
{
    if(!p||!p->model)return MS3D_E_NULL;
    if(mode!=MS3D_PLAYER_ONCE&&mode!=MS3D_PLAYER_LOOP)return MS3D_E_BAD_COUNT;
    if(first>=last||last>p->duration_ms)return MS3D_E_BAD_COUNT;
    p->start_ms=first;p->end_ms=last;p->current_ms=first;
    p->playback_mode=mode;
    p->blend_enabled=0;p->crossfade_active=0;
    return MS3D_OK;
}
int ms3d_player_seek(ms3d_player *p,unsigned long time_ms)
{
    unsigned long len;
    if(!p||!p->model)return MS3D_E_NULL;
    if(p->end_ms<=p->start_ms){p->current_ms=0UL;return MS3D_OK;}
    if(time_ms<=p->start_ms)p->current_ms=p->start_ms;
    else if(time_ms>=p->end_ms){
        if(p->playback_mode==MS3D_PLAYER_LOOP){
            len=p->end_ms-p->start_ms;
            p->current_ms=p->start_ms+(time_ms-p->start_ms)%len;
        } else p->current_ms=p->end_ms;
    } else p->current_ms=time_ms;
    return MS3D_OK;
}
static unsigned long advance_in_range(unsigned long current,
    unsigned long first,unsigned long last,unsigned long dt,int mode)
{
    unsigned long len,offset,step;
    if(first>=last)return current;
    len=last-first;
    if(mode==MS3D_PLAYER_LOOP){
        offset=(current-first)%len;step=dt%len;
        /* Preserve 32-bit unsigned range without offset+step overflow. */
        if(step>=len-offset)return first+(step-(len-offset));
        return first+offset+step;
    }
    if(current>=last)return last;
    return current+(dt>=last-current?last-current:dt);
}
int ms3d_player_advance(ms3d_player *p,unsigned long dt)
{
    if(!p||!p->model)return MS3D_E_NULL;
    if(p->end_ms<=p->start_ms)return MS3D_OK;
    p->current_ms=advance_in_range(p->current_ms,p->start_ms,p->end_ms,
                    dt,p->playback_mode);
    if(p->crossfade_active){
        unsigned long remaining=p->crossfade_duration_ms-p->crossfade_elapsed_ms;
        p->blend_secondary_ms=advance_in_range(p->blend_secondary_ms,
            p->target_start_ms,p->target_end_ms,dt,p->playback_mode);
        p->crossfade_elapsed_ms+=(dt>=remaining?remaining:dt);
        p->blend_weight=ms3d_fx_div(milliseconds_fx(p->crossfade_elapsed_ms),
                                      milliseconds_fx(p->crossfade_duration_ms));
        if(p->crossfade_elapsed_ms>=p->crossfade_duration_ms){
            p->current_ms=p->blend_secondary_ms;
            p->start_ms=p->target_start_ms;
            p->end_ms=p->target_end_ms;
            p->crossfade_active=0;
            p->blend_enabled=0;
            p->blend_weight=0L;
        }
    }
    return MS3D_OK;
}
typedef struct ms3d_player_bridge_s {
    const ms3d_player_sink *sink;
    const ms3d_uv_bindings *bindings;
} ms3d_player_bridge;
static int bridge_corner(void *ctx,ms3d_u16 triangle,ms3d_u8 corner,
                         const ms3d_draw_corner *dc)
{
    ms3d_player_bridge *bridge=(ms3d_player_bridge *)ctx;
    int rc=MS3D_OK;
    if(corner==0U&&bridge->sink->begin_triangle){
        rc=bridge->sink->begin_triangle(bridge->sink->user,triangle,
                        bridge->bindings->group[triangle],
                        bridge->bindings->material[triangle]);
        if(rc!=MS3D_OK)return rc;
    }
    if(bridge->sink->corner)
        rc=bridge->sink->corner(bridge->sink->user,triangle,corner,dc);
    return rc;
}
int ms3d_player_rotation_mode(ms3d_player *p,int mode)
{
    if(!p||!p->model)return MS3D_E_NULL;
    if(mode!=MS3D_ROTATION_EULER&&mode!=MS3D_ROTATION_QUAT_NLERP&&
       mode!=MS3D_ROTATION_QUAT_SLERP)return MS3D_E_BAD_COUNT;
    p->rotation_mode=mode;
    return MS3D_OK;
}
int ms3d_player_blend(ms3d_player *p,unsigned long second_ms,ms3d_fx weight)
{
    if(!p||!p->model)return MS3D_E_NULL;
    if(second_ms>p->duration_ms||weight<0L||weight>MS3D_FX_ONE)
        return MS3D_E_BAD_COUNT;
    p->blend_secondary_ms=second_ms;
    p->blend_weight=weight;
    p->blend_enabled=1;p->crossfade_active=0;
    return MS3D_OK;
}
int ms3d_player_crossfade_to_range(ms3d_player *p,
    unsigned long start,unsigned long end,unsigned long duration)
{
    if(!p||!p->model)return MS3D_E_NULL;
    if(start>=end||end>p->duration_ms||duration==0UL ||
        duration>32767000UL)return MS3D_E_BAD_COUNT;
    p->target_start_ms=start;p->target_end_ms=end;
    p->blend_secondary_ms=start;p->blend_enabled=1;
    p->blend_weight=0L;
    p->crossfade_elapsed_ms=0UL;
    p->crossfade_duration_ms=duration;
    p->crossfade_active=1;
    return MS3D_OK;
}
int ms3d_player_blend_clear(ms3d_player *p)
{
    if(!p||!p->model)return MS3D_E_NULL;
    p->blend_enabled=0;p->blend_weight=0L;p->blend_secondary_ms=0UL;
    p->crossfade_active=0;p->crossfade_elapsed_ms=0UL;
    return MS3D_OK;
}
int ms3d_player_emit(ms3d_player *p,const ms3d_player_sink *sink)
{
    ms3d_fx time;
    unsigned i;
    int rc;
    ms3d_player_bridge bridge;
    ms3d_shaded_sink shaded;
    if(!p||!p->model||!p->pose||!p->vertices||!p->normals||
       !p->bindings||!sink||!sink->corner)return MS3D_E_NULL;
    time=milliseconds_fx(p->current_ms);
    if(p->blend_enabled)
        rc=ms3d_skin_evaluate_blend(p->model,p->pose,time,
                  milliseconds_fx(p->blend_secondary_ms),
                  p->blend_weight,p->rotation_mode);
    else rc=ms3d_skin_evaluate_mode(p->model,p->pose,time,p->rotation_mode);
    if(rc!=MS3D_OK)return rc;
    rc=ms3d_skin_positions(p->model,p->pose,p->vertices);if(rc!=MS3D_OK)return rc;
    if(sink->begin_frame){rc=sink->begin_frame(sink->user,p->current_ms,time,p->model);
        if(rc!=MS3D_OK)return rc;}
    if(sink->material)for(i=0U;i<p->model->num_materials;++i){
        rc=sink->material(sink->user,(ms3d_u16)i,&p->model->materials[i]);
        if(rc!=MS3D_OK)return rc;
    }
    bridge.sink=sink;bridge.bindings=p->bindings;
    shaded.user=&bridge;shaded.corner=bridge_corner;
    rc=ms3d_emit_skinned_uv_corners(p->model,p->pose,p->vertices,
                                    p->normals,p->bindings,&shaded);
    if(rc!=MS3D_OK)return rc;
    if(sink->end_frame)return sink->end_frame(sink->user);
    return MS3D_OK;
}
