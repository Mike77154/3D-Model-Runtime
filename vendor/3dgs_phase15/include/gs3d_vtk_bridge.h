#ifndef GS3D_VTK_BRIDGE_H_INCLUDED
#define GS3D_VTK_BRIDGE_H_INCLUDED
/* C89 read-only numeric export API for VTK frontend. No conversion files. */
#include "gs3d.h"
int gs3d_vtk_group(unsigned int index, unsigned long *nv,
                   unsigned long *nf, unsigned long *nuv,
                   unsigned int *skins, unsigned int *frames);
int gs3d_vtk_vertex(unsigned int group, unsigned long local, long xyz[3],
                    unsigned short *bone);
int gs3d_vtk_uv(unsigned int group, unsigned long local, long uv[2]);
int gs3d_vtk_triangle(unsigned int group, unsigned long face,
                       unsigned long vertex[3], unsigned long uv[3],
                       unsigned int *material);
int gs3d_vtk_skin(unsigned int group,unsigned int skin,
                   unsigned long *w,unsigned long *h,
                   const unsigned char **rgba);
int gs3d_vtk_skin_info(unsigned int group,unsigned int skin,GS3D_TextureInfo *out);
int gs3d_vtk_bone(unsigned int bone,char name[33],
                  unsigned short *parent,long xyz[3]);
int gs3d_vtk_weights(unsigned int group,unsigned long local,
                     unsigned short *bones,long *weights,
                     unsigned int capacity,unsigned int *count);
int gs3d_vtk_frame_name(unsigned int group,unsigned int frame,char out[17]);
int gs3d_vtk_audit(unsigned int group, unsigned long *degenerate,
                   unsigned long *weighted, unsigned long *influences);
#endif
