#ifndef SS89_ENV_H
#define SS89_ENV_H
/* Standalone Q10000 environment UV projection; no heap, floats or 64-bit. */
int ss89_env_sphere_reflection(int nx,int ny,int nz,int vx,int vy,int vz,int *u,int *v);
int ss89_env_sphere_specular(int nx,int ny,int nz,int lx,int ly,int lz,int *u,int *v);
int ss89_env_light(int nx,int ny,int nz,int lx,int ly,int lz);
#endif
