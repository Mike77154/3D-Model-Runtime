#include "re4c3d/re4_motion_audit.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
/* Minimal valid FCV: 1 block, 3 one-key axes. */
int main(void)
{
 re4_u8 arena_bytes[4096];
 re4_span a,b;re4_arena arena;re4_motion_audit meta;re4_err er;int same;
 /* Three 1-key axes follow the 16-byte header in this synthetic FCV. */
 {
  re4_u8 full[64],altered[64];
  memset(full,0,sizeof(full));full[0]=6;full[2]=1;full[3]=4;full[4]=0;full[5]=2;
  full[8]=64;full[12]=16;
  /* Axis 0 at 16, axis 1 at 32, axis 2 at 48 */
  full[16]=1;full[32]=1;full[48]=1;
  memcpy(altered,full,sizeof(full));altered[53]=99;
  a.data=full;a.size=sizeof(full);b.data=altered;b.size=sizeof(altered);
  re4_arena_init(&arena,arena_bytes,sizeof(arena_bytes));
  er=re4_motion_audit_fcv(a,RE4_ENDIAN_LITTLE,&arena,&meta);
  assert(er==RE4_OK);assert(meta.blocks==1U&&meta.keys==3U);
  assert(meta.ik_blocks==1U&&meta.rotation_blocks==0U);assert(arena.pos==0U);
  er=re4_motion_exact_fcv(a,a,RE4_ENDIAN_LITTLE,&arena,&same);
  assert(er==RE4_OK&&same==1);
  er=re4_motion_exact_fcv(a,b,RE4_ENDIAN_LITTLE,&arena,&same);
  assert(er==RE4_OK&&same==0);
  assert(re4_motion_source_policy("em42","em42",0,0)==RE4_MOTION_NATIVE);
  assert(re4_motion_source_policy("em42","em16",0,1)==RE4_MOTION_UNVERIFIED);
  assert(re4_motion_source_policy("em42","em16",1,0)==RE4_MOTION_UNVERIFIED);
  assert(re4_motion_source_policy("em42","em16",1,1)==RE4_MOTION_SHARED_IDENTICAL);
  assert(re4_motion_source_policy("plaga-head-a","em25",1,0)==RE4_MOTION_UNVERIFIED);
  assert(re4_motion_source_policy("", "em16",1,1)==RE4_MOTION_DENIED);
  full[48]=0; /* invalid axis */
  er=re4_motion_audit_fcv(a,RE4_ENDIAN_LITTLE,&arena,&meta);
  assert(er==RE4_ERR_BAD_FORMAT);assert(arena.pos==0U);
 }
 puts("re4_motion_audit_test PASS");return 0;
}
