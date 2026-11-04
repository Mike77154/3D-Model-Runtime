#include "c4d89.h"
#include <limits.h>
#include <string.h>
/* Native key framing is implemented in c4d89_anim.c, shared by reader and
   sampler. No C4D SDK, heap, float, double or 64-bit arithmetic. */
extern int c4d89_read_native_key(FILE *f,unsigned long offset,C4D89_Key *out);
#define QMAX 2147483647L
#define QMIN (-2147483647L-1L)
static int seekkey(FILE *f,const C4D89_AnimTrack *t,unsigned long n,C4D89_Key *k)
{
 unsigned long off;
 if(n>=t->key_count||t->key_stride==0u)return 0;
 if(n>((unsigned long)LONG_MAX-t->first_key_offset)/(unsigned long)t->key_stride)return 0;
 off=t->first_key_offset+n*(unsigned long)t->key_stride;
 if(off>=t->data_end || off+54ul>t->data_end)return 0;
 return c4d89_read_native_key(f,off,k);
}
/* Fractional unsigned division without 64-bit values. */
static long qratio(unsigned long delta,unsigned long span)
{
 unsigned long rem;long q=0L;unsigned int i;
 if(span==0ul)return 0L;
 if(delta>=span)return C4D89_QONE;
 rem=delta;
 for(i=0u;i<16u;i++){
  q<<=1;
  if(rem>=span-rem){rem-=(span-rem);q|=1L;}
  else rem+=rem;
 }
 return q;
}
/* Constant frame sampling. Clamps beyond first/last key and interpolates
   linearly between adjacent keys; original tangent mode is still unknown. */
static int sample_core(FILE *f,const C4D89_AnimTrack *track,
                               long seconds_q16,long *value_q16,int angle_mode)
{
 C4D89_Key left,right,first,last;
 unsigned long lo,hi,mid;long fraction,delta,change,result;int err=C4D89_OK;
 if(!f||!track||!value_q16||seconds_q16<0L||seconds_q16>QMAX||track->key_count==0ul)
  return C4D89_ERR_CORRUPT;
 if(track->key_stride==0u)return C4D89_ERR_UNSUPPORTED;
 if(!seekkey(f,track,0ul,&first)||
    !seekkey(f,track,track->key_count-1ul,&last)||
    last.time_q16<first.time_q16){err=C4D89_ERR_CORRUPT;goto done;}
 if(seconds_q16<=first.time_q16){*value_q16=first.value_q16;goto done;}
 if(seconds_q16>=last.time_q16){*value_q16=last.value_q16;goto done;}
 lo=0ul;hi=track->key_count-1ul;
 while(hi-lo>1ul){
  mid=lo+(hi-lo)/2ul;
  if(!seekkey(f,track,mid,&right)){err=C4D89_ERR_CORRUPT;goto done;}
  if(seconds_q16<right.time_q16)hi=mid;else lo=mid;
 }
 if(!seekkey(f,track,lo,&left)||!seekkey(f,track,hi,&right)||
    left.time_q16>right.time_q16){err=C4D89_ERR_CORRUPT;goto done;}
 if(left.time_q16==right.time_q16){*value_q16=right.value_q16;goto done;}
 if(seconds_q16<left.time_q16||seconds_q16>right.time_q16){err=C4D89_ERR_CORRUPT;goto done;}
 fraction=qratio((unsigned long)(seconds_q16-left.time_q16),
                 (unsigned long)(right.time_q16-left.time_q16));
 if(angle_mode){
  err=c4d89_angle_lerp_q16(left.value_q16,right.value_q16,fraction,value_q16);
  goto done;
 }
 if((left.value_q16<0L&&right.value_q16>QMAX+left.value_q16)||
    (left.value_q16>0L&&right.value_q16<QMIN+left.value_q16)){
   err=C4D89_ERR_LIMIT;goto done;
 }
 delta=right.value_q16-left.value_q16;
 err=c4d89_fixed_mul(delta,fraction,&change);
 if(err!=C4D89_OK)goto done;
 if((change>0L&&left.value_q16>QMAX-change)||
    (change<0L&&left.value_q16<QMIN-change)){
   err=C4D89_ERR_LIMIT;goto done;
 }
 result=left.value_q16+change;
 *value_q16=result;
 done:return err;
}
int c4d89_sample_track_linear(const char *path,const C4D89_AnimTrack *track,
                               long seconds_q16,long *value_q16)
{
 FILE *f;int rc;
 if(!path)return C4D89_ERR_CORRUPT;
 f=fopen(path,"rb");if(!f)return C4D89_ERR_IO;
 rc=c4d89_sample_track_linear_file(f,track,seconds_q16,value_q16);
 fclose(f);return rc;
}

int c4d89_sample_track_linear_file(FILE *f,const C4D89_AnimTrack *track,
                                   long seconds_q16,long *value_q16)
{
 return sample_core(f,track,seconds_q16,value_q16,0);
}
int c4d89_sample_track_angle_file(FILE *f,const C4D89_AnimTrack *track,
                                  long seconds_q16,long *angle_q16)
{
 if(!track||strncmp(track->channel,"Rotation",8u)!=0)return C4D89_ERR_UNSUPPORTED;
 return sample_core(f,track,seconds_q16,angle_q16,1);
}
int c4d89_sample_track_angle(const char *path,const C4D89_AnimTrack *track,
                             long seconds_q16,long *angle_q16)
{
 FILE *f;int rc;
 if(!path)return C4D89_ERR_CORRUPT;
 f=fopen(path,"rb");if(!f)return C4D89_ERR_IO;
 rc=c4d89_sample_track_angle_file(f,track,seconds_q16,angle_q16);
 fclose(f);return rc;
}
