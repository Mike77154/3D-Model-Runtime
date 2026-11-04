#include "c4d89.h"
#include <stdio.h>
#include <stdlib.h>
static C4D89_AnimIndex idx;
typedef struct Pair {C4D89_Key first,last;unsigned long count;} Pair;
static int capture(void *u,unsigned long n,const C4D89_Key *k)
{
 Pair*p=(Pair*)u;if(n==0ul)p->first=*k;p->last=*k;p->count++;return 1;
}
int main(int argc,char **argv)
{
 unsigned int i,nchecked=0u;int rc;FILE *f;
 if(argc<2)return 2;
 rc=c4d89_scan_animation(argv[1],&idx);if(rc){printf("index error=%d\n",rc);return 1;}
 f=fopen(argv[1],"rb");if(!f)return 1;
 for(i=0u;i<idx.count&&nchecked<8u;i++){
  const C4D89_AnimTrack *t=&idx.tracks[i];Pair pair;
  long a,b,mid,v;
  if(t->key_count<2ul||t->key_stride==0u)continue;
  pair.count=0ul;
  rc=c4d89_stream_track(argv[1],t,capture,&pair);
  if(rc||pair.count!=t->key_count){printf("stream error %u\n",i);return 1;}
  rc=c4d89_sample_track_linear_file(f,t,pair.first.time_q16,&a);
  if(rc||a!=pair.first.value_q16){printf("start mismatch %u\n",i);return 1;}
  rc=c4d89_sample_track_linear_file(f,t,pair.last.time_q16,&b);
  if(rc||b!=pair.last.value_q16){printf("end mismatch %u\n",i);return 1;}
  mid=pair.first.time_q16+(pair.last.time_q16-pair.first.time_q16)/2L;
  rc=c4d89_sample_track_linear_file(f,t,mid,&v);
  if(rc){printf("mid failed %u=%d\n",i,rc);return 1;}
  nchecked++;
 }
 fclose(f);
 printf("native verified strided tracks: %u / %u\n",nchecked,idx.count);
 return nchecked?0:1;
}
