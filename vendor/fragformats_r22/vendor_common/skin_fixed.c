#include "skin_fixed.h"
#include "fixed_matrix.h"
int skin_blend_point(const long base[3],const long matrices[][16],
                     unsigned long matrix_count,const unsigned char bone_ids[4],
                     const long weights[4],long out[3]){
 long accum[3]={0L,0L,0L};long tmp[3];unsigned long i;int k;
 long sum=0L;
 if(!base||!matrices||!bone_ids||!weights||!out)return 0;
 for(i=0UL;i<4UL;i++){
  if(weights[i]<0L||weights[i]>FX_ONE)return 0;
  sum=fx_add(sum,weights[i]);
  if(weights[i]==0L)continue;
  if((unsigned long)bone_ids[i]>=matrix_count)return 0;
  fx_transform_point(matrices[bone_ids[i]],base,tmp);
  for(k=0;k<3;k++)accum[k]=fx_add(accum[k],fx_mul(tmp[k],weights[i]));
 }
 if(sum<FX_ONE-3L||sum>FX_ONE+3L)return 0;
 for(k=0;k<3;k++)out[k]=accum[k];
 return 1;
}
