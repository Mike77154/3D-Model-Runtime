#ifndef SS89_NORMALS_H
#define SS89_NORMALS_H
/* 16-bit H/P decompression and user-provided 8-bit Gouraud lookup. C89/Q15. */
void ss89_normal_hp(unsigned h,unsigned p,int out[3]);
/* File: 256 triplets, signed little endian Q15, 1536 bytes. */
int ss89_gouraud_load(const char *filename);
int ss89_gouraud_ready(void);
int ss89_normal_index(unsigned index,int out[3]);
#endif
