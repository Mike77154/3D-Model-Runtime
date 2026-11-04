#include "c4d89.h"
#include <stdio.h>
#include <string.h>
/* Small native-record fixture. No real scene is invented: tests only the
   frame consumer against precisely defined C4D6 54-byte key records. */
static const unsigned char sig[13]={0x0c,0x00,0x02,0x01,0x00,0x01,0xaf,
0x50,0,0,0,0,0x16};
static void put32(unsigned char *p,unsigned long u)
{
 p[0]=(unsigned char)(u>>24);p[1]=(unsigned char)(u>>16);
 p[2]=(unsigned char)(u>>8);p[3]=(unsigned char)u;
}
static int writekey(FILE *f,long offset,unsigned long time_bits,unsigned long value_bits)
{
 unsigned char b[54];memset(b,0,sizeof(b));memcpy(b,sig,13u);
 put32(b+13,time_bits);put32(b+17,0x3f800000ul);b[21]=0x16;
 b[30]=0x16;b[39]=0x13;put32(b+40,value_bits);
 b[44]=0x13;b[49]=0x13;
 if(fseek(f,offset,SEEK_SET)!=0)return 0;
 return fwrite(b,1u,54u,f)==54u;
}
typedef struct {unsigned int count;unsigned int ids[4];long values[4];int stop;} Result;
static int capture(void *u,unsigned int i,const C4D89_AnimTrack *t,long value)
{
 Result *r=(Result*)u;(void)t;
 if(r->count>=4u)return 0;
 r->ids[r->count]=i;r->values[r->count]=value;r->count++;
 return !r->stop;
}
static C4D89_AnimIndex index_table; /* Must be static; > several MB. */
int main(void)
{
 FILE*f=tmpfile();C4D89_FrameStats st;Result r;int rc;
 if(!f)return 2;
 if(!writekey(f,128L,0ul,0ul) ||
    !writekey(f,237L,0x3f800000ul,0x3f800000ul) ||
    !writekey(f,512L,0ul,0x40000000ul) ||
    !writekey(f,621L,0x3f800000ul,0x40800000ul)){fclose(f);return 2;}
 memset(&index_table,0,sizeof(index_table));index_table.count=3u;
 strcpy(index_table.tracks[0].channel,"Position . X");
 index_table.tracks[0].first_key_offset=128ul;
 index_table.tracks[0].data_end=291ul;
 index_table.tracks[0].key_count=2ul;
 index_table.tracks[0].key_stride=109u;
 strcpy(index_table.tracks[1].channel,"Rotation . H");
 index_table.tracks[1].first_key_offset=512ul;
 index_table.tracks[1].data_end=675ul;
 index_table.tracks[1].key_count=2ul;
 index_table.tracks[1].key_stride=109u;
 strcpy(index_table.tracks[2].channel,"Position . Y");
 index_table.tracks[2].key_count=3ul;index_table.tracks[2].key_stride=0u;
 memset(&r,0,sizeof(r));
 rc=c4d89_sample_animation_frame_file(f,&index_table,32768L,0u,capture,&r,&st);
 if(rc||st.sampled!=2u||st.unsupported!=1u||r.count!=2u||
    r.ids[0]!=0u||r.ids[1]!=1u||r.values[0]!=32768L||r.values[1]!=196608L){
    printf("frame mismatch rc=%d sample=%u unsupported=%u count=%u x=%ld r=%ld\n",
      rc,st.sampled,st.unsupported,r.count,r.values[0],r.values[1]);
    fclose(f);return 1;
 }
 memset(&r,0,sizeof(r));
 rc=c4d89_sample_animation_frame_file(f,&index_table,32768L,1u,capture,&r,&st);
 if(rc||st.sampled!=2u||r.count!=2u||r.values[1]!=196608L){fclose(f);return 1;}
 memset(&r,0,sizeof(r));r.stop=1;
 rc=c4d89_sample_animation_frame_file(f,&index_table,32768L,0u,capture,&r,&st);
 if(rc!=C4D89_ERR_SINK || r.count!=1u){fclose(f);return 1;}
 fclose(f);puts("v0.13 scene-frame sampling passed (2 tracks, skip, short-arc, callback abort)");
 return 0;
}
