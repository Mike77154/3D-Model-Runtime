#include "c4d89.h"
#include <string.h>
#include <limits.h>
/* XC4DC4D6 animation records recovered from several game scenes.
   Each key: 12-byte signature (C4D parameter 0x1AF50) followed by
   3x [0x16, binary32 numerator, binary32 denominator] and
   3x [0x13, binary32 value].
   The 1st time rational and 1st scalar carry the key's time and value.
   The other four numbers are retained in file but their interpretation
   (tangents/interpolation) is NOT established. Therefore do not claim
   Bézier interpolation or complete animation fidelity.
*/
static const unsigned char key_signature[13]={
 0x0c,0x00,0x02,0x01,0x00,0x01,0xaf,0x50,0x00,0x00,0x00,0x00,0x16};
static unsigned long be32(const unsigned char*b)
{ return ((unsigned long)b[0]<<24)|((unsigned long)b[1]<<16)|
         ((unsigned long)b[2]<<8)|b[3]; }
static long qtime(long num,long den)
{
    unsigned long n,d,whole,rem,frac=0ul;unsigned int bit;
    if(num<0L||den<=0L)return -1L;
    n=(unsigned long)num;d=(unsigned long)den;
    whole=n/d;rem=n%d;
    if(whole>(unsigned long)LONG_MAX/65536ul)return -1L;
    for(bit=0u;bit<16u;bit++) {
        frac<<=1;
        if(rem>=d-rem){rem=rem-(d-rem);frac|=1ul;}
        else rem+=rem;
    }
    return (long)(whole*65536ul+frac);
}
int c4d89_read_native_key(FILE *f,unsigned long offset,C4D89_Key *out)
{
    unsigned char b[54];long num,den,v,t;
    if(offset>(unsigned long)LONG_MAX)return 0;
    if(fseek(f,(long)offset,SEEK_SET)!=0||fread(b,1u,54u,f)!=54u)return 0;
    if(memcmp(b,key_signature,13u)!=0||
       b[21]!=0x16||b[30]!=0x16||b[39]!=0x13||
       b[44]!=0x13||b[49]!=0x13)return 0;
    if(c4d89_be_float_to_q16(b+13,&num)!=0||
       c4d89_be_float_to_q16(b+17,&den)!=0||
       c4d89_be_float_to_q16(b+40,&v)!=0)return 0;
    t=qtime(num,den);
    if(t<0L||t>7200L*65536L)return 0;
    out->file_offset=offset;out->time_q16=t;out->value_q16=v;
    out->time_numerator_q16=num;out->time_denominator_q16=den;
    return 1;
}
/* Type name tokens: 0x82 + BE32 nbytes + printable UTF16BE.
   Other scene strings use 0x83,0x83 and must also delimit tracks. */
static int typed_string(FILE *f,long at,long end,char *buffer,unsigned int *nbytes)
{
    unsigned char b[4];unsigned long sz;int c,sec;unsigned int i,n,start;
    if(fseek(f,at,SEEK_SET)!=0)return 0;
    c=fgetc(f);if(c!=0x82&&c!=0x83)return 0;
    start=1u;
    if(c==0x83){sec=fgetc(f);if(sec!=0x83)return 0;start=2u;}
    if(fread(b,1u,4u,f)!=4u)return 0;
    sz=be32(b);
    if(sz<4ul||sz>2ul*(C4D89_MAX_NAME-1u)||(sz&1ul)||
       sz>(unsigned long)(end-at- (long)start-4L))return 0;
    n=(unsigned int)(sz/2ul);
    for(i=0u;i<n;i++) {
        int a=fgetc(f),z=fgetc(f);
        if(a!=0||z<32||z>126)return 0;
        buffer[i]=(char)z;
    }
    buffer[n]='\0';*nbytes=start+4u+(unsigned int)sz;
    return 1;
}
static int channel(const char*s)
{
    const char *p=0;unsigned int len;
    if(strncmp(s,"Position . ",11u)==0)p=s+11;
    else if(strncmp(s,"Rotation . ",11u)==0)p=s+11;
    else if(strncmp(s,"Scale . ",8u)==0)p=s+8;
    if(!p)return 0;
    len=(unsigned int)strlen(p);
    return len==1u && strchr("XYZHPB",p[0])!=0;
}
static int scan_keys(FILE *f,C4D89_AnimTrack *t)
{
    unsigned long pos,prev=0ul;C4D89_Key k;long last=-1L;
    unsigned int stride=0u;
    /* The smallest record in this version uses 54 bytes. */
    if(t->data_start>(unsigned long)LONG_MAX ||
       fseek(f,(long)t->data_start,SEEK_SET)!=0)return C4D89_ERR_IO;
    for(pos=t->data_start;pos+54ul<=t->data_end;pos++) {
        int c=fgetc(f);
        if(c==EOF)break;
        if(c!=0x0c)continue;
        if(!c4d89_read_native_key(f,pos,&k)){fseek(f,(long)pos+1L,SEEK_SET);continue;}
        if(last>k.time_q16)continue;
        if(t->key_count==0ul){t->first_key_offset=pos;
            t->start_time_q16=k.time_q16;t->first_value_q16=k.value_q16;}
        else {unsigned long diff=pos-prev;
            if(t->key_count==1ul)stride=diff<=65535ul?(unsigned int)diff:0u;
            else if(diff!=(unsigned long)stride)stride=0u;}
        t->key_count++;t->end_time_q16=k.time_q16;
        t->last_value_q16=k.value_q16;last=k.time_q16;prev=pos;
        pos+=53ul;
        fseek(f,(long)pos+1L,SEEK_SET);
    }
    t->key_stride=stride;
    return C4D89_OK;
}
int c4d89_scan_animation(const char *path,C4D89_AnimIndex *idx)
{
    FILE*f;long size,pos;char buf[C4D89_MAX_NAME];unsigned int len,i;
    char last_name[C4D89_MAX_NAME];char owner[C4D89_MAX_NAME];
    unsigned long last_name_at=0ul,owner_at=0ul,joint_at=0ul;
    unsigned int owner_valid=0u;
    last_name[0]='\0';owner[0]='\0';
    if(!path||!idx)return C4D89_ERR_CORRUPT;
    memset(idx,0,sizeof(*idx));
    f=fopen(path,"rb");if(!f)return C4D89_ERR_IO;
    {
        unsigned char magic[8];
        if(fread(magic,1u,8u,f)!=8u ||
           (memcmp(magic,"XC4DC4D6",8u)!=0 &&
            memcmp(magic,"QC4DC4D6",8u)!=0)){
            fclose(f);return C4D89_ERR_SIGNATURE;
        }
    }
    if(fseek(f,0L,SEEK_END)!=0||(size=ftell(f))<8L){fclose(f);return C4D89_ERR_IO;}
    for(pos=8L;pos<size-10L;) {
        if(typed_string(f,pos,size,buf,&len)) {
            if(idx->count>0u && idx->tracks[idx->count-1u].data_end==0ul)
                idx->tracks[idx->count-1u].data_end=(unsigned long)pos;
            if(strcmp(buf,"Joint")==0) {
                /* Observed node layout: human name preceding typed Joint.
                   Evidence only: not an authoritative scene graph edge. */
                owner_valid=0u;
                if(last_name[0] && last_name_at<(unsigned long)pos &&
                   (unsigned long)pos-last_name_at<=500ul) {
                    strcpy(owner,last_name);
                    owner_at=last_name_at;joint_at=(unsigned long)pos;
                    owner_valid=1u;
                }
            } else if(channel(buf)) {
                if(idx->count<C4D89_MAX_TRACKS) {
                    C4D89_AnimTrack *t=&idx->tracks[idx->count++];
                    memset(t,0,sizeof(*t));strcpy(t->channel,buf);
                    t->name_offset=(unsigned long)pos;
                    if(owner_valid) {
                        strcpy(t->owner_hint,owner);
                        t->owner_name_offset=owner_at;
                        t->owner_joint_offset=joint_at;
                        t->owner_confidence=1u;
                    }
                    t->data_start=(unsigned long)pos+(unsigned long)len;
                } else idx->omitted_tracks++;
            } else if(strcmp(buf,"Skin")!=0 && strcmp(buf,"Weight")!=0 &&
                      strcmp(buf,"Polygon")!=0 && strcmp(buf,"UVW")!=0 &&
                      strcmp(buf,"Phong")!=0 && strcmp(buf,"Texture")!=0 &&
                      strcmp(buf,"Tracks")!=0) {
                strcpy(last_name,buf);last_name_at=(unsigned long)pos;
            }
            pos+=(long)len;
        } else pos++;
    }
    if(idx->count>0u && idx->tracks[idx->count-1u].data_end==0ul)
        idx->tracks[idx->count-1u].data_end=(unsigned long)size;
    for(i=0u;i<idx->count;i++) {
        if(idx->tracks[i].data_end==0ul)idx->tracks[i].data_end=(unsigned long)size;
        if(idx->tracks[i].data_end>idx->tracks[i].data_start)
            scan_keys(f,&idx->tracks[i]);
        idx->decoded_keys+=idx->tracks[i].key_count;
    }
    fclose(f);return C4D89_OK;
}
int c4d89_stream_track(const char *path,const C4D89_AnimTrack *t,
                       C4D89_KeyFn cb,void *user)
{
    FILE *f;unsigned long pos,count=0ul;long last=-1L;C4D89_Key k;
    if(!path||!t||!cb)return C4D89_ERR_CORRUPT;
    f=fopen(path,"rb");if(!f)return C4D89_ERR_IO;
    if(t->data_start>(unsigned long)LONG_MAX ||
       fseek(f,(long)t->data_start,SEEK_SET)!=0){fclose(f);return C4D89_ERR_IO;}
    for(pos=t->data_start;pos+54ul<=t->data_end;pos++) {
        int c=fgetc(f);
        if(c==EOF)break;
        if(c!=0x0c)continue;
        if(!c4d89_read_native_key(f,pos,&k)){fseek(f,(long)pos+1L,SEEK_SET);continue;}
        if(k.time_q16<last){fseek(f,(long)pos+1L,SEEK_SET);continue;}
        if(!cb(user,count++,&k)){fclose(f);return C4D89_ERR_SINK;}
        last=k.time_q16;pos+=53ul;
        fseek(f,(long)pos+1L,SEEK_SET);
    }
    fclose(f);
    return count==t->key_count?C4D89_OK:C4D89_ERR_CORRUPT;
}
