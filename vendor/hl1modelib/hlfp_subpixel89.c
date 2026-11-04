#include "hlfp_subpixel89.h"
#include <limits.h>
#include <string.h>

#define SP89_COORD_LIMIT 524288L
#define SP89_MAX_PRODUCT ((long)INT_MAX / 2L)

static long sp89_floor16(long v) {
    long q = v / 16L;
    if (v % 16L < 0L) --q;
    return q;
}
static long sp89_ceil16(long v) { return -sp89_floor16(-v); }
static long sp89_edge(long ax, long ay, long bx, long by,
                      long px, long py) {
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}
static int sp89_topleft(long ax, long ay, long bx, long by,
                        int orientation) {
    long dx = bx - ax, dy = by - ay;
    if (orientation > 0) { dx = -dx; dy = -dy; }
    /* screen origin upper-left, +y goes down; canonical winding normalized */
    return dy < 0L || (dy == 0L && dx > 0L);
}
int hlfp_sp89_prepare(hlfp_sp89_triangle *t,
                      int x0, int y0, int x1, int y1, int x2, int y2) {
    long mnx, mxx, mny, mxy, dx, dy;
    long a;
    if (!t) return HLFP_ERR_BAD_ARGUMENT;
    memset(t, 0, sizeof(*t));
    t->x[0]=x0;t->x[1]=x1;t->x[2]=x2;
    t->y[0]=y0;t->y[1]=y1;t->y[2]=y2;
    mnx = x0;mxx=x0;mny=y0;mxy=y0;
#define SP89_BOUNDS(i) do { \
 if ((long)t->x[i]<mnx) mnx=t->x[i]; \
 if ((long)t->x[i]>mxx) mxx=t->x[i]; \
 if ((long)t->y[i]<mny) mny=t->y[i]; \
 if ((long)t->y[i]>mxy) mxy=t->y[i]; \
} while(0)
    SP89_BOUNDS(1);SP89_BOUNDS(2);
#undef SP89_BOUNDS
    if (mnx < -SP89_COORD_LIMIT || mxx > SP89_COORD_LIMIT ||
        mny < -SP89_COORD_LIMIT || mxy > SP89_COORD_LIMIT)
        return HLFP_ERR_OVERFLOW;
    dx = mxx-mnx+16L;dy=mxy-mny+16L;
    /* Reserve enough headroom for subtraction of two signed products.
     * This bound is independent of 32-vs-64-bit C long size. */
    if (dx<=0L || dy<=0L || dx>SP89_MAX_PRODUCT/dy)
        return HLFP_ERR_OVERFLOW;
    a=sp89_edge(x0,y0,x1,y1,x2,y2);
    if (a == 0L) return HLFP_OK;
    t->area=a;
    t->orientation=a>0L?1:-1;
    t->edge_inclusive[0]=sp89_topleft(x1,y1,x2,y2,t->orientation);
    t->edge_inclusive[1]=sp89_topleft(x2,y2,x0,y0,t->orientation);
    t->edge_inclusive[2]=sp89_topleft(x0,y0,x1,y1,t->orientation);
    t->min_x=(int)sp89_ceil16(mnx-8L);
    t->max_x=(int)sp89_floor16(mxx-8L);
    t->min_y=(int)sp89_ceil16(mny-8L);
    t->max_y=(int)sp89_floor16(mxy-8L);
    t->valid=(t->min_x<=t->max_x && t->min_y<=t->max_y);
    return HLFP_OK;
}
int hlfp_sp89_sample(const hlfp_sp89_triangle *t,int x,int y,
                     long weights[3]) {
    long px,py,v;
    int k;
    if(!t || !t->valid || x<t->min_x || x>t->max_x ||
       y<t->min_y || y>t->max_y) return 0;
    px=(long)x*16L+8L;py=(long)y*16L+8L;
    for(k=0;k<3;++k){
        int a=(k+1)%3,b=(k+2)%3;
        v=sp89_edge(t->x[a],t->y[a],t->x[b],t->y[b],px,py);
        if(t->orientation<0){
            if(v>0L || (v==0L&&!t->edge_inclusive[k]))return 0;
        }else{
            if(v<0L || (v==0L&&!t->edge_inclusive[k]))return 0;
        }
        if(weights)weights[k]=v;
    }
    return 1;
}
