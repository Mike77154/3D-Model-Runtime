#include <stdio.h>
#include <string.h>
#include "ss89settings.h"
/* Read only the known tagged filename fields. Never evaluate resource paths. */
static unsigned readle(const unsigned char *a)
{
    return (unsigned)a[0]|((unsigned)a[1]<<8)|((unsigned)a[2]<<16)|((unsigned)a[3]<<24);
}
static int name_at(const unsigned char *data,unsigned size,unsigned i,char *out)
{
    unsigned j,n;
    if(i>size||size-i<8U||memcmp(data+i,"DFNM",4))return 0;
    n=readle(data+i+4);
    if(n>=260U||n>size-i-8U)return 0;
    for(j=0;j<n;j++){
        unsigned char c=data[i+8U+j];
        if(c<32U||c>126U||c==':'||c=='/'||c=='*'||c=='?'||c=='<'||c=='>')return 0;
        out[j]=(c=='\\')?'/':(char)c;
    }
    out[n]=0;
    if(strstr(out,"../")||strstr(out,"/..")||!strncmp(out,"..",2))return 0;
    return 1;
}
int ss89_settings_read(const char *file,SS89_Settings *out)
{
    /* Typical WTEX model-settings files are small. Explicit cap guards input. */
    static unsigned char buf[1048576];
    FILE *fp; long len;unsigned n,i;int seen=0;
    if(!file||!out)return 0;
    memset(out,0,sizeof(*out));
    fp=fopen(file,"rb");if(!fp)return 0;
    if(fseek(fp,0,SEEK_END)||(len=ftell(fp))<0||len<8||len>(long)sizeof(buf)||fseek(fp,0,SEEK_SET)){fclose(fp);return 0;}
    n=(unsigned)len;
    if(fread(buf,1,n,fp)!=n){fclose(fp);return 0;}
    fclose(fp);
    if(memcmp(buf,"WTEX",4))return 0;
    for(i=4U;i+12U<=n;i++){
        char *dest=NULL;
        if(!memcmp(buf+i,"FXTS",4))dest=out->specular;
        else if(!memcmp(buf+i,"FXTR",4))dest=out->reflection;
        else if(!memcmp(buf+i,"FXTB",4))dest=out->bump;
        if(dest){if(!name_at(buf,n,i+4U,dest))return 0;seen=1;}
    }
    /* WTEX variants may have no secondary maps. */
    (void)seen;
    return 1;
}
