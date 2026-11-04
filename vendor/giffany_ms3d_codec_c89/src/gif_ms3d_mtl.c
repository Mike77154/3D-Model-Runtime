#include "gif_ms3d_mtl.h"
#include <stdio.h>
#include <string.h>

#define CHUNK_CAP 16384
static unsigned char g_copybuf[CHUNK_CAP];

const char *ms3d_mtl_basename(const char *str)
{
    const char *p,*base;
    if (!str) return "";
    base=str;
    for(p=str;*p!='\0';++p) if(*p=='/'||*p=='\\')base=p+1;
    return base;
}
static int path_dir(const char *path,char *out,size_t n)
{
    size_t len,i,last=0U;
    if(!path||!out||n==0U)return 0;
    len=strlen(path);
    for(i=0U;i<len;++i)if(path[i]=='/'||path[i]=='\\')last=i+1U;
    if(last>=n)return 0;
    if(!last) { if(n<2U)return 0;out[0]='.';out[1]='\0'; }
    else {memcpy(out,path,last);out[last]='\0';}
    return 1;
}
static int join(char *dst,size_t capacity,const char *dir,const char *base)
{
    size_t a,b;
    if(!dst||!dir||!base)return 0;
    a=strlen(dir);b=strlen(base);
    if(a+b+2U>capacity)return 0;
    memcpy(dst,dir,a);
    if(a && dir[a-1U]!='/' && dir[a-1U]!='\\')dst[a++]='/';
    memcpy(dst+a,base,b+1U);
    return 1;
}
void ms3d_mtl_filename(const char *obj_path,char *mtl_path,size_t cap)
{
    size_t len;char *dot,*slash,*back;
    if(!mtl_path||cap==0U)return;
    mtl_path[0]='\0';if(!obj_path)return;
    len=strlen(obj_path);
    if(len+5U>=cap)return;
    strcpy(mtl_path,obj_path);
    dot=strrchr(mtl_path,'.');slash=strrchr(mtl_path,'/');back=strrchr(mtl_path,'\\');
    if(back && (!slash||back>slash))slash=back;
    if(dot && (!slash||dot>slash))*dot='\0';
    strcat(mtl_path,".mtl");
}
static int copy_bytes(const char *from,const char *to)
{
    FILE *f,*g; size_t got;int success=1;
    if(!from||!to)return 0;
    f=fopen(from,"rb");if(!f)return 0;
    g=fopen(to,"wb");if(!g){fclose(f);return 0;}
    while((got=fread(g_copybuf,1U,sizeof(g_copybuf),f))>0U)
        if(fwrite(g_copybuf,1U,got,g)!=got){success=0;break;}
    if(ferror(f))success=0;
    if(fclose(g)!=0)success=0;
    fclose(f);
    if(!success)remove(to);
    return success;
}
static int file_exists(const char *path)
{
    FILE *f=fopen(path,"rb");if(!f)return 0;fclose(f);return 1;
}
static int copy_map(const char *path,const char *root,const char *model_path,
                    const char *output_dir,unsigned id,const char *prefix,
                    char output_name[96])
{
    const char *base,*ext;char src[MS3D_MTL_PATH_CAP];
    char dst[MS3D_MTL_PATH_CAP],model_dir[MS3D_MTL_PATH_CAP];
    int found=0;
    output_name[0]='\0';
    if(!path||!*path)return 0;
    base=ms3d_mtl_basename(path);
    if(!*base)return 0;
    ext=strrchr(base,'.');if(!ext||strlen(ext)>12U)ext=".dat";
    if(strlen(prefix)+strlen(ext)+16U>=96U)return 0;
    sprintf(output_name,"ms3d_%s_%u%s",prefix,id,ext);
    if(!join(dst,sizeof(dst),output_dir,output_name))return 0;
    if(root && *root) {
        /* prefer the embedded relative folder if it exists under root */
        if(join(src,sizeof(src),root,path) && file_exists(src))found=1;
        if(!found && join(src,sizeof(src),root,base) && file_exists(src))found=1;
    }
    if(!found && path_dir(model_path,model_dir,sizeof(model_dir))) {
        if(join(src,sizeof(src),model_dir,base) && file_exists(src))found=1;
    }
    if(!found||!copy_bytes(src,dst)) {output_name[0]='\0';return 0;}
    return 1;
}
static void number(FILE *f,ms3d_fx val)
{
    char buf[48];
    if(ms3d_fx_to_ascii(val,buf,sizeof(buf),5U)==MS3D_OK)fputs(buf,f);
    else fputc('0',f);
}
int ms3d_mtl_export(const char *mtl_path,const char *ms3d_path,
                    const char *image_root,const ms3d_model *m,
                    ms3d_mtl_bundle *info)
{
    FILE *f;unsigned i,j,k;char outdir[MS3D_MTL_PATH_CAP];
    char stem[40],prefix[60];const char *bn;
    const ms3d_material *mat;
    if(!mtl_path||!ms3d_path||!m||!info)return 0;
    if(!path_dir(mtl_path,outdir,sizeof(outdir)))return 0;
    bn=ms3d_mtl_basename(mtl_path);
    for(k=0U;k<sizeof(stem)-1U && bn[k] && bn[k]!='.';++k) {
        char ch=bn[k];
        if((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||
           (ch>='0'&&ch<='9')||ch=='_'||ch=='-')stem[k]=ch;
        else stem[k]='_';
    }
    stem[k]='\0';
    if(!k)strcpy(stem,"model");
    memset(info,0,sizeof(*info));
    f=fopen(mtl_path,"wb");if(!f)return 0;
    fputs("# MilkShape 3D v1.4 | texture binding per material\n",f);
    for(i=0U;i<m->num_materials;++i) {
        mat=&m->materials[i];
        fprintf(f,"newmtl ms3d_mat_%u\nKa",i);
        for(j=0U;j<3U;++j) {fputc(' ',f);number(f,mat->ambient[j]);}
        fputs("\nKd",f);
        for(j=0U;j<3U;++j) {fputc(' ',f);number(f,mat->diffuse[j]);}
        fputs("\nKs",f);
        for(j=0U;j<3U;++j) {fputc(' ',f);number(f,mat->specular[j]);}
        fputs("\nKe",f);
        for(j=0U;j<3U;++j) {fputc(' ',f);number(f,mat->emissive[j]);}
        fputs("\nNs ",f);number(f,mat->shininess);
        fputs("\nd ",f);number(f,mat->transparency);
        fputs("\nillum 2\n",f);
        if(mat->texture[0]) {
            sprintf(prefix,"%s_diffuse",stem);
            if(copy_map(mat->texture,image_root,ms3d_path,outdir,i,prefix,info->texture[i])){
                fprintf(f,"map_Kd %s\n",info->texture[i]);++info->copied_diffuse;
            }else{
                ++info->missing_diffuse;
                fprintf(f,"# MISSING texture original: %s\n",mat->texture);
            }
        }
        if(mat->alphamap[0]) {
            sprintf(prefix,"%s_alpha",stem);
            if(copy_map(mat->alphamap,image_root,ms3d_path,outdir,i,prefix,info->alpha[i])){
                fprintf(f,"map_d %s\n",info->alpha[i]);++info->copied_alpha;
            }else{
                ++info->missing_alpha;
                fprintf(f,"# MISSING alphamap original: %s\n",mat->alphamap);
            }
        }
        fputc('\n',f);
    }
    /* Always define the fallback material for unassigned triangles. */
    fputs("newmtl ms3d_untextured\nKa 0.2 0.2 0.2\nKd 0.75 0.75 0.75\nKs 0 0 0\nd 1\nillum 2\n",f);
    return fclose(f)==0;
}
