#include "hod2_character_clips.h"
#include <stdio.h>
int main(void){unsigned int v,t,k,id,p;unsigned long count=0UL;
 for(v=0U;v<2U;v++)for(t=0U;t<HOD2_CHARACTER_TYPES;t++)for(k=0U;k<2U;k++){
  if(hod2_character_clip(v,t,k,&id,&p)){
   if(id==0U || p==HOD2_CLIP_NONE)return 1;
   count++;
  }
 }
 if(!hod2_character_clip(0U,1U,0U,&id,&p) || p!=HOD2_CLIP_EXE_DISPATCH)return 2;
 if(hod2_character_clip(0U,65U,2U,&id,&p) || p!=HOD2_CLIP_BANK_STRIDE_CANDIDATE)return 3;
 printf("HOD2 C89 clip catalogue PASS: %lu assigned entries across editions\n",count);
 return 0;}
