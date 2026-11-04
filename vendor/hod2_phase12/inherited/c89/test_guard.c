#include "hod2_resource_guard.h"
#include <stdio.h>
int main(void){
 if(!hod2_uv_bits_finite(0x3f800000UL))return 1;
 if(hod2_uv_bits_finite(0x7f800000UL))return 2;
 if(hod2_uv_bits_finite(0x7fc00000UL))return 3;
 if(!hod2_uv_bits_finite(0xbf800000UL))return 4;
 if(!hod2_texture_span(128UL,128UL,2UL,3UL,4096UL,12000UL))return 5;
 if(hod2_texture_span(256UL,256UL,2UL,3UL,6000UL,7000UL))return 6;
 if(!hod2_mot_clip_span(59UL,16UL,4UL+((59UL-1UL)/2UL+2UL)*108UL))return 7;
 if(hod2_mot_clip_span(59UL,16UL,123UL))return 8;
 puts("HOD2 guard C89 PASS");return 0;
}
