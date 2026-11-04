#include "../gs3d_internal.h"
/* Skin payload sizing is independent of decoding: unsupported compression is kept as a blob. */
int gs_skin_payload(unsigned long base,unsigned long w,unsigned long h,
                    unsigned int type,unsigned long *bytes) {
    unsigned long n,payload=0UL,mips=0UL;unsigned int bpp=0U;
    unsigned int kind=type&15U;
    (void)base;
    if(kind==1U){*bytes=0UL;return 0;} /* MDL7 material reference */
    if(kind==7U){*bytes=0UL;return 0;} /* external filename handled separately */
    if(kind==6U){*bytes=w;return 0;} /* DDS: byte length stored as width */
    if(kind==0U)bpp=1U;
    else if(kind==2U || kind==3U)bpp=2U;
    else if(kind==4U)bpp=3U;
    else if(kind==5U)bpp=4U;
    else if(kind==10U || kind==11U)bpp=2U;
    else if(kind==12U)bpp=3U;
    else if(kind==13U)bpp=4U;
    else return GS3D_ERR_UNSUPPORTED;
    if(w && h>ULONG_MAX/w)return GS3D_ERR_RANGE;
    n=w*h;
    if(n>ULONG_MAX/(unsigned long)bpp)return GS3D_ERR_RANGE;
    payload=n*(unsigned long)bpp;
    if(kind>=10U){
        mips=(n>>2)+(n>>4)+(n>>6);
        if(mips> (ULONG_MAX-payload)/(unsigned long)bpp)return GS3D_ERR_RANGE;
        payload+=mips*(unsigned long)bpp;
    }
    *bytes=payload;
    return 0;
}
