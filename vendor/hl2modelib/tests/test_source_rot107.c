/* Source RadianEuler rotation regression: PI must be a proper half-turn.
   Compile strict C89, no float/double/heap/64-bit. */
#include "hl2modelib.h"
#include <stdio.h>
#include <string.h>
static hl2m_ik_pose input;
static hl2m_pose_matrices output;
static int ab(int x){return x<0?-x:x;}
static int dot(int a,int b,int c,int d,int e,int f)
{return (a/256)*(d/256)+(b/256)*(e/256)+(c/256)*(f/256);}
int main(void)
{
    int r, i, n, err, greatest=0;
    const int angles[]={0,51471,102943,154415,205887,-205887,-154415};
    memset(&input,0,sizeof(input));
    input.bone_count=1;input.parent[0]=-1;
    for(i=0;i<(int)(sizeof(angles)/sizeof(angles[0]));i++) {
        input.rot[0].x=angles[i];
        input.rot[0].y=angles[(i+3)%7];
        input.rot[0].z=angles[(i+5)%7];
        r=hl2m_pose_matrices_from_pose(&input,1,&output);
        if(r!=HL2M_OK)return 1;
        for(n=0;n<3;n++){
            hl2m_mat3x4 *m=&output.local[0];
            int a=dot(m->m[0][n],m->m[1][n],m->m[2][n],
                      m->m[0][n],m->m[1][n],m->m[2][n]);
            err=ab(a-HL2M_FIXED_ONE);
            if(err>greatest)greatest=err;
            if(err>2200){printf("INVALID rotation anglecase=%d axis=%d error=%d\n",i,n,err);return 2;}
        }
    }
    memset(&input,0,sizeof(input));input.bone_count=1;input.parent[0]=-1;
    input.rot[0].y=205887;
    r=hl2m_pose_matrices_from_pose(&input,1,&output);
    if(r!=HL2M_OK)return 3;
    if(ab(output.local[0].m[0][2])>500 || ab(output.local[0].m[2][0])>500) return 4;
    if(ab(output.local[0].m[0][0]+HL2M_FIXED_ONE)>500)return 5;
    printf("Source RadianEuler PI/quadrant rotation test OK, worst error Q16=%d\n",greatest);
    return 0;
}
