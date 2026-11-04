#include "c4d89.h"
#include <string.h>
#include <limits.h>

/* Experimental native C4D6 Joint record inventory. A named record directly
   preceding a typed 'Joint' token is an ownership HINT; a matrix block after
   that typed token is observable but not an inverse-bind assertion.
   This scans the original .c4d file and does not require the Maxon SDK. */
static unsigned long be32(const unsigned char *b)
{
    return ((unsigned long)b[0]<<24)|((unsigned long)b[1]<<16)|
           ((unsigned long)b[2]<<8)|(unsigned long)b[3];
}
static int read_typed(FILE *f,long at,long end,char *buffer,unsigned int *len)
{
    unsigned char b[4];unsigned long sz;unsigned int i,n,head;
    int c;
    if(fseek(f,at,SEEK_SET)!=0)return 0;
    c=fgetc(f);if(c!=0x82&&c!=0x83)return 0;
    head=1u;
    if(c==0x83){if(fgetc(f)!=0x83)return 0;head=2u;}
    if(fread(b,1u,4u,f)!=4u)return 0;
    sz=be32(b);
    if(sz<2ul||sz>(C4D89_MAX_NAME-1u)*2ul||(sz&1ul)||
       sz>(unsigned long)(end-at-(long)head-4L))return 0;
    n=(unsigned int)(sz/2ul);
    for(i=0u;i<n;i++){
        int a=fgetc(f),z=fgetc(f);
        if(a!=0||z<32||z>126)return 0;
        buffer[i]=(char)z;
    }
    buffer[n]='\0';*len=head+4u+(unsigned int)sz;
    return 1;
}
static int ignored_name(const char *s)
{
    return strcmp(s,"Skin")==0||strcmp(s,"Weight")==0||
      strcmp(s,"Texture")==0||strcmp(s,"UVW")==0||
      strcmp(s,"Phong")==0||strcmp(s,"Polygon")==0||
      strcmp(s,"Tracks")==0||strcmp(s,"Joint")==0||
      strstr(s,"Position . ")==s||strstr(s,"Rotation . ")==s||
      strstr(s,"Scale . ")==s;
}
static int read_matrix(FILE *f,long at,long file_size,
                       C4D89_JointRecord *record)
{
    unsigned char b[49];long q[12];long seek;
    unsigned int i;
    /* All examined samples have the matrix as a typed 0x19 4x3 array
       within 120 bytes after the Joint name. Do not treat as bind pose. */
    for(seek=at+15L;seek<at+150L&&seek+49L<=file_size;seek++) {
        if(fseek(f,seek,SEEK_SET)!=0||fread(b,1u,49u,f)!=49u)return 0;
        if(b[0]!=0x19)continue;
        for(i=0u;i<12u;i++) {
            if(c4d89_be_float_to_q16(b+1u+i*4u,&q[i])!=C4D89_OK)break;
            if(q[i]<-2097152L||q[i]>2097152L)break; /* <32 unit scale */
        }
        if(i!=12u)continue;
        /* The matrix must not be all zero, and have 3 nonempty basis cols. */
        if((q[3]==0L&&q[4]==0L&&q[5]==0L)||
           (q[6]==0L&&q[7]==0L&&q[8]==0L)||
           (q[9]==0L&&q[10]==0L&&q[11]==0L))continue;
        memcpy(record->matrix_q16,q,sizeof(q));
        record->matrix_offset=(unsigned long)seek;
        record->has_matrix=1u;
        return 1;
    }
    return 0;
}
int c4d89_scan_joint_records(const char *path,C4D89_JointTable *table)
{
    FILE *f;long end,pos;unsigned int len;
    char token[C4D89_MAX_NAME],last_name[C4D89_MAX_NAME];
    unsigned long last_name_at=0ul;
    unsigned char magic[8];
    if(!path||!table)return C4D89_ERR_CORRUPT;
    memset(table,0,sizeof(*table));last_name[0]='\0';
    f=fopen(path,"rb");if(!f)return C4D89_ERR_IO;
    if(fread(magic,1u,8u,f)!=8u ||
       (memcmp(magic,"XC4DC4D6",8u)!=0 &&
        memcmp(magic,"QC4DC4D6",8u)!=0)){
        fclose(f);return C4D89_ERR_SIGNATURE;
    }
    if(fseek(f,0L,SEEK_END)!=0||(end=ftell(f))<8L){
        fclose(f);return C4D89_ERR_IO;
    }
    for(pos=8L;pos<end-10L;) {
        if(read_typed(f,pos,end,token,&len)) {
            if(strcmp(token,"Joint")==0) {
                if(last_name[0] && last_name_at<(unsigned long)pos &&
                   (unsigned long)pos-last_name_at<=500ul) {
                    if(table->count<C4D89_MAX_JOINT_RECORDS) {
                        C4D89_JointRecord *r=&table->joints[table->count++];
                        strcpy(r->name,last_name);
                        r->name_offset=last_name_at;
                        r->joint_offset=(unsigned long)pos;
                        if(read_matrix(f,pos,end,r))table->with_matrix++;
                    } else table->omitted++;
                }
            } else if(!ignored_name(token)) {
                strcpy(last_name,token);
                last_name_at=(unsigned long)pos;
            }
            pos+=(long)len;
        } else pos++;
    }
    fclose(f);
    return C4D89_OK;
}
