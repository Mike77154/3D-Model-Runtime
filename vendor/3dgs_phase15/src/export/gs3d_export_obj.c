#include "../../include/gs3d.h"
#include <stdio.h>
#include <string.h>
/* Diagnostic OBJ/MTL/TGA exporter; no heap and no FP operations. */
static int gs3d_fprintq(FILE *f,long q){
    unsigned long u;
    if(q<0L){ if(fputc('-',f)==EOF)return 0;u=(unsigned long)(-(q+1L))+1UL; }
    else u=(unsigned long)q;
    return fprintf(f,"%lu.%04lu",u>>16,((u&65535UL)*10000UL+32768UL)>>16)>0;
}
extern int gs3d_save_tga(const char *path,unsigned long w,unsigned long h);
int gs3d_export_obj(const char *prefix){
    FILE *obj,*mtl;char objp[300],mtlp[300],texp[300];
    const char *leaf,*slash,*back;
    unsigned int gi,si;unsigned long j,k,w,h;GS3D_Group *g;GS3D_MaterialInfo mi;
    unsigned int last_mat,mat;
    GS3D_Vec *v;GS3D_UV *uv;GS3D_Face *face;
    if(!gs3d_model.format || !prefix || strlen(prefix)>230U)return GS3D_ERR_RANGE;
    sprintf(objp,"%s.obj",prefix);sprintf(mtlp,"%s.mtl",prefix);
    slash=strrchr(prefix,'/');back=strrchr(prefix,'\\');
    leaf=slash && (!back || slash>back) ? slash+1 : (back ? back+1:prefix);
    obj=fopen(objp,"wb");if(!obj)return GS3D_ERR_IO;
    mtl=fopen(mtlp,"wb");if(!mtl){fclose(obj);return GS3D_ERR_IO;}
    fprintf(obj,"# Original GS3D C89 exporter\nmtllib %s.mtl\n",leaf);
    for(gi=0U;gi<gs3d_model.groups;gi++){
        g=&gs3d_model.group[gi];
        for(j=0;j<g->vertex_count;j++){
            v=&gs3d_model.v[g->vertex_first+j];
            fputs("v ",obj);gs3d_fprintq(obj,v->x);fputc(' ',obj);
            gs3d_fprintq(obj,v->y);fputc(' ',obj);gs3d_fprintq(obj,v->z);fputc('\n',obj);
        }
    }
    for(gi=0U;gi<gs3d_model.groups;gi++){
        g=&gs3d_model.group[gi];
        for(j=0;j<g->uv_count;j++){
            uv=&gs3d_model.uv[g->uv_first+j];
            fputs("vt ",obj);gs3d_fprintq(obj,uv->u);fputc(' ',obj);
            gs3d_fprintq(obj,uv->v);fputc('\n',obj);
        }
    }
    for(gi=0U;gi<gs3d_model.groups;gi++){
        g=&gs3d_model.group[gi];
        fprintf(obj,"g group_%u_%s\n",gi,g->name);
        if(!g->skin_count){
            fprintf(mtl,"newmtl group_%u_default\nKd 0.75 0.75 0.75\n",gi);
        }
        for(si=0U;si<g->skin_count;si++){
            if(gs3d_get_material(gi,si,&mi)){
                fclose(obj);fclose(mtl);return GS3D_ERR_RANGE;
            }
            fprintf(mtl,"newmtl group_%u_skin_%u\nKd ",gi,si);
            gs3d_fprintq(mtl,mi.diffuse[0]);fputc(' ',mtl);
            gs3d_fprintq(mtl,mi.diffuse[1]);fputc(' ',mtl);
            gs3d_fprintq(mtl,mi.diffuse[2]);fputc('\n',mtl);
            fputs("d ",mtl);gs3d_fprintq(mtl,mi.opacity);fputc('\n',mtl);
            if(gs3d_decode_skin(gi,si,&w,&h)==0){
                sprintf(texp,"%s_group%u_skin%u.tga",prefix,gi,si);
                if(gs3d_save_tga(texp,w,h)){
                    fclose(obj);fclose(mtl);return GS3D_ERR_IO;
                }
                fprintf(mtl,"map_Kd %s_group%u_skin%u.tga\n",leaf,gi,si);
            }
        }
        last_mat=65535U;
        for(j=0;j<g->face_count;j++){
            face=&gs3d_model.face[g->face_first+j];
            mat=0U;
            if(g->skin_count && face->material < g->skin_count)
                mat=face->material;
            if(mat!=last_mat){
                if(g->skin_count)fprintf(obj,"usemtl group_%u_skin_%u\n",gi,mat);
                else fprintf(obj,"usemtl group_%u_default\n",gi);
                last_mat=mat;
            }
            fputs("f",obj);
            for(k=0;k<3UL;k++){
                unsigned long c=2UL-k; /* 3DGS winding parity */
                unsigned long vi=g->vertex_first+(unsigned long)face->vi[c]+1UL;
                unsigned long ti=g->uv_first+(unsigned long)face->ti[c]+1UL;
                if(g->uv_count)fprintf(obj," %lu/%lu",vi,ti);
                else fprintf(obj," %lu",vi);
            }
            fputc('\n',obj);
        }
    }
    fclose(obj);fclose(mtl);return 0;
}
