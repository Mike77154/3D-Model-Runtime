/* Software preview for extracted RTCW materials. Original CC0-1.0 ANSI C89.
   Orthographic isometric projection, affine UV, per-triangle depth test, alpha test.
   Not a full RTCW shader pipeline / GPU renderer. No heap or floating point. */
#include "rtcw_codec.h"
#include "rtcw_texrender.h"
#include "rtcw_texture.h"
#include "rtcw_material.h"
#include "rtcw_texcache.h"
#include "rtcw_shaderstyle.h"
#include "rtcw_stagefx.h"
#include "rtcw_transorder.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#define WIDTH 640
#define HEIGHT 640
static unsigned char frame_buffer[WIDTH*HEIGHT*3UL];
static long depth_buffer[WIDTH*HEIGHT];
static char renderer_err[192]="OK";
static unsigned int missing;
static unsigned int partial_shaders;
typedef struct render_material_s {
    char path[768];
    rtcw_shaderstyle style;
    rtcw_fx fx;
    char overlay[RTCW_FX_MAX_STAGES][768];
} render_material;
static render_material surface_mat[RTCW_MAX_SURFACES];
unsigned int rtcw_render_partial_shaders(void){return partial_shaders;}
static int rend_err(const char *s){strncpy(renderer_err,s,191);renderer_err[191]=0;return 0;}
const char *rtcw_render_error(void){return renderer_err;}
unsigned int rtcw_render_missing_textures(void){return missing;}
static long edge(long ax,long ay,long bx,long by,long px,long py){return (px-ax)*(by-ay)-(py-ay)*(bx-ax);}
static long wrap(long n,long bound){n%=bound;return n<0?n+bound:n;}
static void put_rgb(unsigned long pos,unsigned char r,unsigned char g,unsigned char b){
    frame_buffer[pos*3UL]=r;frame_buffer[pos*3UL+1]=g;frame_buffer[pos*3UL+2]=b;
}
static int projection(void){
    unsigned long i;long minx=LONG_MAX,miny=LONG_MAX,maxx=LONG_MIN,maxy=LONG_MIN;
    long x,y,w,h,den;rtcw_vertex *v;
    if(!rtcw_current.vertex_count)return rend_err("empty model");
    for(i=0;i<rtcw_current.vertex_count;i++){
        v=&rtcw_current.verts[i];
        if(v->x< -1000000L||v->x>1000000L||
           v->y< -1000000L||v->y>1000000L||
           v->z< -1000000L||v->z>1000000L)
            return rend_err("model coordinates outside preview limits");
        x=v->x-v->y;y=(v->x+v->y)/2L-v->z;
        v->sx=x;v->sy=y;
        if(x<minx)minx=x;
        if(x>maxx)maxx=x;
        if(y<miny)miny=y;
        if(y>maxy)maxy=y;
    }
    w=maxx-minx;h=maxy-miny;den=w>h?w:h;if(den<1)den=1;
    for(i=0;i<rtcw_current.vertex_count;i++){
        v=&rtcw_current.verts[i];
        v->sx=60+(v->sx-minx)*520L/den+(520L-w*520L/den)/2L;
        v->sy=60+(v->sy-miny)*520L/den+(520L-h*520L/den)/2L;
    }
    return 1;
}
/* On 32-bit C89 long, restrict intermediate integer products. */
/* Two passes: depth-writing opaque and sorted translucent geometry. */
static int draw_triangle(const rtcw_triangle *t,const rtcw_texture *texture,int pass,
                         const rtcw_shaderstyle *style,const rtcw_fx_stage *fx,
                         unsigned long now_ms,int overlay){
    const rtcw_vertex *a=&rtcw_current.verts[t->a],*b=&rtcw_current.verts[t->b],*c=&rtcw_current.verts[t->c];
    long area=edge(a->sx,a->sy,b->sx,b->sy,c->sx,c->sy);
    long minx,maxx,miny,maxy,x,y,w0,w1,w2,q0,q1,q2,u,v,z,z0,z1,z2,tu,tv;
    unsigned long pos,tp;
    unsigned int texw=texture?texture->width:0U,texh=texture?texture->height:0U;
    unsigned int alpha,r,g,bl,inv;int opaque;
    if(area==0)return 1;
    if(area<0)area=-area;
    minx=a->sx;if(b->sx<minx)minx=b->sx;if(c->sx<minx)minx=c->sx;
    maxx=a->sx;if(b->sx>maxx)maxx=b->sx;if(c->sx>maxx)maxx=c->sx;
    miny=a->sy;if(b->sy<miny)miny=b->sy;if(c->sy<miny)miny=c->sy;
    maxy=a->sy;if(b->sy>maxy)maxy=b->sy;if(c->sy>maxy)maxy=c->sy;
    if(minx<0)minx=0;
    if(miny<0)miny=0;
    if(maxx>=WIDTH)maxx=WIDTH-1;
    if(maxy>=HEIGHT)maxy=HEIGHT-1;
    if(minx>maxx||miny>maxy)return 1;
    if(area>1000000L)return 1;
    if(a->u< -65536L||a->u>65536L||b->u< -65536L||b->u>65536L||c->u< -65536L||c->u>65536L||
       a->v< -65536L||a->v>65536L||b->v< -65536L||b->v>65536L||c->v< -65536L||c->v>65536L)return 1;
    z0=(a->x/64L)+(a->y/64L)+(a->z/64L);
    z1=(b->x/64L)+(b->y/64L)+(b->z/64L);
    z2=(c->x/64L)+(c->y/64L)+(c->z/64L);
    for(y=miny;y<=maxy;y++)for(x=minx;x<=maxx;x++){
        w0=edge(b->sx,b->sy,c->sx,c->sy,x,y);
        w1=edge(c->sx,c->sy,a->sx,a->sy,x,y);
        w2=edge(a->sx,a->sy,b->sx,b->sy,x,y);
        if(!((w0>=0&&w1>=0&&w2>=0)||(w0<=0&&w1<=0&&w2<=0)))continue;
        if(w0<0){w0=-w0;w1=-w1;w2=-w2;}
        q1=w1*1024L/area;q2=w2*1024L/area;q0=1024L-q1-q2;
        pos=(unsigned long)y*(unsigned long)WIDTH+(unsigned long)x;
        z=(q0*z0+q1*z1+q2*z2)/1024L;
        if(z<depth_buffer[pos])continue;
        u=a->u+(q1*(b->u-a->u)+q2*(c->u-a->u))/1024L;
        v=a->v+(q1*(b->v-a->v)+q2*(c->v-a->v))/1024L;
        if(fx){
            u=rtcw_fx_uv(u,fx->scale_s,fx->scroll_s,now_ms);
            v=rtcw_fx_uv(v,fx->scale_t,fx->scroll_t,now_ms);
        }
        alpha=255U;
        if(texture && texw && texh){
            /* Wrap Q12 before texel multiply to avoid signed-32 overflow. */
            tu=(wrap(u,4096L)*(long)texw)/4096L;
            tv=(wrap(v,4096L)*(long)texh)/4096L;
            tp=((unsigned long)tv*(unsigned long)texw+(unsigned long)tu)*4UL;
            alpha=(unsigned int)texture->pixel[tp+3UL];
        }else{tp=0UL;}
        if(fx)alpha=(alpha*fx->alpha+127U)/255U;
        if(style->alpha_func==RTCW_ALPHA_GT0&&alpha==0U)continue;
        if(style->alpha_func==RTCW_ALPHA_GE128&&alpha<128U)continue;
        if(style->alpha_func==RTCW_ALPHA_LT128&&alpha>=128U)continue;
        if(style->alpha_func==RTCW_ALPHA_NONE&&alpha==0U)continue;
        /* Alpha test is a binary discard, not a blending request. */
        opaque=!overlay&&style->blend==RTCW_BLEND_AUTO&&
               (alpha==255U||style->alpha_func!=RTCW_ALPHA_NONE);
        if(pass==0&&!opaque)continue;
        if(pass==1&&opaque)continue;
        if(!texture||!texw||!texh){
            int check=(((int)wrap(u/256L,2L))^((int)wrap(v/256L,2L)))&1;
            if(pass==0)put_rgb(pos,check?222U:49U,check?50U:27U,check?156U:72U);
        }else if(pass==0){
            r=(unsigned int)texture->pixel[tp];
            g=(unsigned int)texture->pixel[tp+1UL];
            bl=(unsigned int)texture->pixel[tp+2UL];
            if(fx){r=(r*fx->rgb[0]+127U)/255U;
                   g=(g*fx->rgb[1]+127U)/255U;
                   bl=(bl*fx->rgb[2]+127U)/255U;}
            put_rgb(pos,(unsigned char)r,(unsigned char)g,(unsigned char)bl);
        }else{
            unsigned int rr=frame_buffer[pos*3UL],gg=frame_buffer[pos*3UL+1UL],bb=frame_buffer[pos*3UL+2UL];
            unsigned int sr=(unsigned int)texture->pixel[tp];
            unsigned int sg=(unsigned int)texture->pixel[tp+1UL];
            unsigned int sb=(unsigned int)texture->pixel[tp+2UL];
            if(fx){sr=(sr*fx->rgb[0]+127U)/255U;
                   sg=(sg*fx->rgb[1]+127U)/255U;
                   sb=(sb*fx->rgb[2]+127U)/255U;}
            if(style->blend==RTCW_BLEND_ADD){
                r=rr+(sr*alpha+127U)/255U;
                g=gg+(sg*alpha+127U)/255U;
                bl=bb+(sb*alpha+127U)/255U;
                if(r>255U)r=255U;
                if(g>255U)g=255U;
                if(bl>255U)bl=255U;
            }else if(style->blend==RTCW_BLEND_FILTER){
                r=(rr*((sr*alpha+255U*(255U-alpha))/255U)+127U)/255U;
                g=(gg*((sg*alpha+255U*(255U-alpha))/255U)+127U)/255U;
                bl=(bb*((sb*alpha+255U*(255U-alpha))/255U)+127U)/255U;
            }else{
                inv=255U-alpha;
                r=(sr*alpha+rr*inv+127U)/255U;
                g=(sg*alpha+gg*inv+127U)/255U;
                bl=(sb*alpha+bb*inv+127U)/255U;
            }
            put_rgb(pos,(unsigned char)r,(unsigned char)g,(unsigned char)bl);
        }
        if(pass==0)depth_buffer[pos]=z;
    }
    return 1;
}
static int resolve_materials(const char *root,const char *shader_file,
                             const char *mapfile,unsigned long now_ms){
    FILE *map=NULL;unsigned int s,j;char record[4096],shader_path[768];
    char mapped_overlays[RTCW_FX_MAX_STAGES][768];
    char *field,*name,*source,*shader_ref,*tail;unsigned long row;
    const rtcw_texture *tex;
    int rc;
    missing=0U;partial_shaders=0U;
    if(mapfile){map=fopen(mapfile,"rb");if(!map)return rend_err("cannot read VFS material map");}
    for(s=0U;s<rtcw_current.surface_count;s++){
        memset(&surface_mat[s].fx,0,sizeof(surface_mat[s].fx));
        memset(surface_mat[s].overlay,0,sizeof(surface_mat[s].overlay));
        surface_mat[s].style.blend=RTCW_BLEND_AUTO;
        surface_mat[s].style.alpha_func=RTCW_ALPHA_NONE;
        surface_mat[s].style.stage_count=0U;
        surface_mat[s].style.unhandled=0U;
        shader_path[0]=0;
        memset(mapped_overlays,0,sizeof(mapped_overlays));
        if(map){
            if(!fgets(record,sizeof(record),map)){fclose(map);return rend_err("material map has too few rows");}
            field=strtok(record,"\t\r\n");name=strtok(NULL,"\t\r\n");
            source=strtok(NULL,"\t\r\n");shader_ref=strtok(NULL,"\t\r\n");
            if(!field||!name||!source){fclose(map);return rend_err("invalid material map row");}
            /* v0.18: accept legacy 3/4 column maps and new 7 column maps. */
            tail=strtok(NULL,"\t\r\n");
            if(tail){
                for(j=1U;j<RTCW_FX_MAX_STAGES;j++){
                    if(strlen(tail)>=sizeof(mapped_overlays[j])){
                        fclose(map);return rend_err("VFS stage image path too long");
                    }
                    if(strcmp(tail,"-")!=0)strcpy(mapped_overlays[j],tail);
                    tail=strtok(NULL,"\t\r\n");
                    if(j<RTCW_FX_MAX_STAGES-1U && !tail){
                        fclose(map);return rend_err("incomplete VFS stage columns");
                    }
                }
                if(tail){fclose(map);return rend_err("extra VFS stage columns");}
            }
            row=0UL;
            while(*field>='0'&&*field<='9'){
                row=row*10UL+(unsigned long)(*field-'0');field++;
                if(row>RTCW_MAX_SURFACES)break;
            }
            if(*field||row!=(unsigned long)s||strcmp(name,rtcw_current.surfaces[s].shader)){
                fclose(map);return rend_err("material map surface mismatch");
            }
            if(strlen(source)>=sizeof(surface_mat[s].path)){fclose(map);return rend_err("texture path too long");}
            strcpy(surface_mat[s].path,source);
            if(shader_ref&&strcmp(shader_ref,"-")!=0){
                if(strlen(shader_ref)>=sizeof(shader_path)){fclose(map);return rend_err("shader path too long");}
                strcpy(shader_path,shader_ref);
            }
        }else{
            if(!rtcw_material_path_time(root,shader_file,rtcw_current.surfaces[s].shader,
                                        now_ms,surface_mat[s].path,sizeof(surface_mat[s].path)))
                surface_mat[s].path[0]=0;
            if(shader_file&&strcmp(shader_file,"-")!=0){
                if(strlen(shader_file)>=sizeof(shader_path))return rend_err("shader path too long");
                strcpy(shader_path,shader_file);
            }
        }
        if(shader_path[0]){
            int result=rtcw_shaderstyle_lookup(shader_path,rtcw_current.surfaces[s].shader,
                                               &surface_mat[s].style);
            if(result<0){if(map)fclose(map);return rend_err(rtcw_shaderstyle_error());}
        }
        if(shader_path[0]){
            rc=rtcw_fx_lookup(shader_path,rtcw_current.surfaces[s].shader,now_ms,
                              &surface_mat[s].fx);
            if(rc<0){if(map)fclose(map);return rend_err(rtcw_fx_error());}
            if(rc==1){
                rtcw_fx *fx=&surface_mat[s].fx;
                if(fx->stages>0U){
                    surface_mat[s].style.blend=fx->stage[0].blend;
                    surface_mat[s].style.alpha_func=fx->stage[0].alpha_func;
                }
                partial_shaders+=fx->unhandled;
                for(j=1U;j<fx->stages;j++){
                    if(mapped_overlays[j][0]){
                        strcpy(surface_mat[s].overlay[j],mapped_overlays[j]);
                    }else if(root&&fx->stage[j].map[0]&&
                       rtcw_material_path_time(root,NULL,fx->stage[j].map,0UL,
                           surface_mat[s].overlay[j],sizeof(surface_mat[s].overlay[j]))){
                        /* Stage has a resolvable local texture. */
                    }else{
                        partial_shaders++;
                        surface_mat[s].overlay[j][0]=0;
                    }
                }
            }else partial_shaders+=surface_mat[s].style.unhandled;
        }else partial_shaders+=surface_mat[s].style.unhandled;
        tex=(surface_mat[s].path[0]&&strcmp(surface_mat[s].path,"-")!=0)?
            rtcw_texcache_get(surface_mat[s].path):NULL;
        if(!tex)missing++;
    }
    if(map){
        if(fgets(record,sizeof(record),map)){fclose(map);return rend_err("material map has extra rows");}
        if(ferror(map)||fclose(map)!=0)return rend_err("material map read/close failed");
    }
    return 1;
}
static int render_generic(const char *outppm,const char *root,const char *shader_file,
                          const char *mapfile,unsigned long now_ms){
    FILE *out;unsigned long i;const rtcw_transitem *sorted;
    const rtcw_texture *tex;unsigned int s,j;const rtcw_triangle *t;
    rtcw_shaderstyle overlay_style;
    const rtcw_fx_stage *first_fx;
    if(!outppm||(!root&&!mapfile))return rend_err("missing output or root");
    if(rtcw_current.surface_count>RTCW_MAX_SURFACES ||
       rtcw_current.triangle_count>RTCW_MAX_TRIANGLES)return rend_err("model capacity exceeded");
    if(!projection())return 0;
    if(!resolve_materials(root,shader_file,mapfile,now_ms))return 0;
    for(i=0;i<WIDTH*HEIGHT;i++){put_rgb(i,16,19,31);depth_buffer[i]=LONG_MIN;}
    if(!rtcw_transorder_build(rtcw_current.triangle_count))return rend_err("invalid triangle index in sort");
    for(s=0U;s<rtcw_current.surface_count;s++){
        tex=(surface_mat[s].path[0]&&strcmp(surface_mat[s].path,"-")!=0)?
            rtcw_texcache_get(surface_mat[s].path):NULL;
        first_fx=surface_mat[s].fx.stages?&surface_mat[s].fx.stage[0]:NULL;
        for(i=rtcw_current.surfaces[s].first_triangle;
            i<rtcw_current.surfaces[s].first_triangle+rtcw_current.surfaces[s].triangles;i++){
            if(i>=rtcw_current.triangle_count)return rend_err("invalid surface triangle range");
            if(!draw_triangle(&rtcw_current.tris[i],tex,0,&surface_mat[s].style,first_fx,now_ms,0))return rend_err("opaque draw failed");
        }
    }
    /* Single global translucent queue, ordered far to near irrespective of surfaces. */
    sorted=rtcw_transorder_items();
    for(i=0;i<rtcw_transorder_count();i++){
        t=&rtcw_current.tris[sorted[i].triangle];s=t->surface;
        if(s>=rtcw_current.surface_count)return rend_err("invalid triangle surface index");
        tex=(surface_mat[s].path[0]&&strcmp(surface_mat[s].path,"-")!=0)?
            rtcw_texcache_get(surface_mat[s].path):NULL;
        first_fx=surface_mat[s].fx.stages?&surface_mat[s].fx.stage[0]:NULL;
        if(tex && !draw_triangle(t,tex,1,&surface_mat[s].style,first_fx,now_ms,0))
            return rend_err("translucent draw failed");
        for(j=1U;j<surface_mat[s].fx.stages;j++){
            if(!surface_mat[s].overlay[j][0])continue;
            tex=rtcw_texcache_get(surface_mat[s].overlay[j]);
            if(!tex){partial_shaders++;continue;}
            overlay_style.blend=surface_mat[s].fx.stage[j].blend;
            overlay_style.alpha_func=surface_mat[s].fx.stage[j].alpha_func;
            overlay_style.stage_count=1U;overlay_style.unhandled=0U;
            if(!draw_triangle(t,tex,1,&overlay_style,&surface_mat[s].fx.stage[j],now_ms,1))
                return rend_err("overlay draw failed");
        }
    }
    out=fopen(outppm,"wb");if(!out)return rend_err("cannot create textured PPM");
    fprintf(out,"P6\n%d %d\n255\n",WIDTH,HEIGHT);
    if(fwrite(frame_buffer,1,WIDTH*HEIGHT*3UL,out)!=WIDTH*HEIGHT*3UL){fclose(out);return rend_err("PPM output truncated");}
    if(fclose(out)!=0)return rend_err("PPM close failed");
    strcpy(renderer_err,"OK");return 1;
}

int rtcw_render_textured(const char *outppm,const char *root,const char *shader_file){
    return render_generic(outppm,root,shader_file,NULL,0UL);
}
int rtcw_render_textured_map(const char *outppm,const char *mapfile){
    return render_generic(outppm,NULL,NULL,mapfile,0UL);
}

int rtcw_render_textured_time(const char *outppm,const char *root,const char *shader_file,
                              unsigned long now_ms){
    return render_generic(outppm,root,shader_file,NULL,now_ms);
}
int rtcw_render_textured_map_time(const char *outppm,const char *mapfile,unsigned long now_ms){
    return render_generic(outppm,NULL,NULL,mapfile,now_ms);
}
