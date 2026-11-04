#include "mdl7_color.h"
#define GS7_Q16_ONE 65536L
static unsigned long gs7_round_channel(long value){
 unsigned long v;
 if(value<=0L)return 0UL;
 if(value>=GS7_Q16_ONE)return 255UL;
 v=((unsigned long)value*255UL+32768UL)/65536UL;
 return v>255UL?255UL:v;
}
unsigned long gs7_pack4_q16(const long channels[4]){
 unsigned long c0=gs7_round_channel(channels[0]);
 unsigned long c1=gs7_round_channel(channels[1]);
 unsigned long c2=gs7_round_channel(channels[2]);
 unsigned long c3=gs7_round_channel(channels[3]);
 return c0|(c1<<8)|(c2<<16)|(c3<<24);
}
