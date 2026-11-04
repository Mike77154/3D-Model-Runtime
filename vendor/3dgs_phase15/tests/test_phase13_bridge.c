#include "../include/gs3d_vtk_bridge.h"
#include <stdio.h>
#include <string.h>
#define CHECK(X) do {if(!(X)){fprintf(stderr,"FAILED %s:%d: %s\n",__FILE__,__LINE__,#X);return 1;}} while(0)
int main(void){
    unsigned long nv,nf,nuv,w,h,tri[3],uvix[3],deg,weighted,inf;
    unsigned int skins,frames,mat;
    unsigned short bone,parents[12];long weights[12];unsigned int nc;
    long xyz[3],uv[2];
    const unsigned char *rgba;
    char name[17];
    CHECK(gs3d_vtk_group(0U,&nv,&nf,&nuv,&skins,&frames)==GS3D_ERR_RANGE);
    CHECK(gs3d_vtk_vertex(0U,0UL,xyz,&bone)==GS3D_ERR_RANGE);
    CHECK(gs3d_vtk_triangle(0U,0UL,tri,uvix,&mat)==GS3D_ERR_RANGE);
    CHECK(gs3d_open("tests/fixtures/mdl7_postgroup_weighted.mdl")==GS3D_OK);
    CHECK(gs3d_vtk_group(0U,&nv,&nf,&nuv,&skins,&frames)==GS3D_OK);
    CHECK(nv>0UL&&nf>0UL&&gs3d_model.groups==1U);
    CHECK(gs3d_vtk_audit(0U,&deg,&weighted,&inf)==GS3D_OK);
    CHECK(weighted>0UL&&inf>=weighted);
    CHECK(gs3d_vtk_weights(0U,0UL,parents,weights,12U,&nc)==GS3D_OK);
    CHECK(nc<=12U);
    CHECK(gs3d_vtk_weights(0U,nv,parents,weights,12U,&nc)==GS3D_ERR_RANGE);
    CHECK(gs3d_vtk_vertex(0U,0UL,xyz,&bone)==GS3D_OK);
    CHECK(gs3d_vtk_triangle(0U,0UL,tri,uvix,&mat)==GS3D_OK);
    CHECK(tri[0]<nv&&tri[1]<nv&&tri[2]<nv);
    CHECK(gs3d_vtk_vertex(0U,nv,xyz,&bone)==GS3D_ERR_RANGE);
    CHECK(gs3d_vtk_triangle(0U,nf,tri,uvix,&mat)==GS3D_ERR_RANGE);
    CHECK(gs3d_vtk_group(1U,&nv,&nf,&nuv,&skins,&frames)==GS3D_ERR_RANGE);
    CHECK(gs3d_vtk_skin(0U,skins,&w,&h,&rgba)==GS3D_ERR_RANGE);
    CHECK(gs3d_vtk_frame_name(0U,frames,name)==GS3D_ERR_RANGE);
    gs3d_close();
    CHECK(gs3d_open("tests/fixtures/mdl7_unused_second_uv.mdl")==GS3D_OK);
    CHECK(gs3d_vtk_group(0U,&nv,&nf,&nuv,&skins,&frames)==GS3D_OK);
    CHECK(nuv>0UL);
    CHECK(gs3d_vtk_uv(0U,0UL,uv)==GS3D_OK);
    CHECK(gs3d_vtk_uv(0U,nuv,uv)==GS3D_ERR_RANGE);
    CHECK(gs3d_vtk_audit(0U,&deg,&weighted,&inf)==GS3D_OK);
    gs3d_close();
    CHECK(gs3d_open("tests/fixtures/mdl7_skeletal_sequence.mdl")==GS3D_OK);
    CHECK(gs3d_vtk_group(0U,&nv,&nf,&nuv,&skins,&frames)==GS3D_OK);
    CHECK(frames>=2U);
    CHECK(gs3d_vtk_frame_name(0U,0U,name)==GS3D_OK);
    CHECK(strlen(name)<=16U);
    gs3d_close();
    puts("phase13 bridge pass: bounded groups, vertices, faces, UVs, weights, skins, frame names");
    return 0;
}
