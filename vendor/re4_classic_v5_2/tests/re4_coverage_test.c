#include "re4c3d/re4c3d.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void)
{
    re4_pmd_model m;re4_pmd_name names[3];re4_pmd_skeleton bones[3];
    re4_fcv f;re4_fcv_block blocks[5];re4_clip_coverage cov;re4_err e;
    memset(&m,0,sizeof(m));memset(&names,0,sizeof(names));
    memset(&bones,0,sizeof(bones));memset(&f,0,sizeof(f));
    memset(blocks,0,sizeof(blocks));
    names[0].name.ptr="born__00";names[0].name.len=8;
    names[1].name.ptr="other_01";names[1].name.len=8;
    names[2].name.ptr="born__15";names[2].name.len=8;
    m.skeleton_count=3;m.skeletons=bones;m.skeleton_name_count=3;m.skeleton_names=names;
    f.block_count=5;f.blocks=blocks;
    blocks[0].bone_type=2;blocks[0].bone_no=0;
    blocks[1].bone_type=2;blocks[1].bone_no=15;
    blocks[2].bone_type=2;blocks[2].bone_no=15; /* duplicate channel */
    blocks[3].bone_type=2;blocks[3].bone_no=7;
    blocks[4].bone_type=4;blocks[4].bone_no=0; /* special role */
    e=re4_character_clip_coverage(&m,&f,&cov);assert(e==RE4_OK);
    assert(cov.name_mapped_bones==2 && cov.skeleton_bones==3);
    assert(cov.rotation_channels==4 && cov.linked_rotation_channels==3);
    assert(cov.unique_linked_bones==2 && cov.unmatched_rotation_channels==1);
    assert(cov.special_channels==1);
    assert(re4_character_clip_coverage(0,&f,&cov)==RE4_ERR_NULL);
    f.blocks=0;
    assert(re4_character_clip_coverage(&m,&f,&cov)==RE4_ERR_BAD_FORMAT);
    /* Regression: shared matcher must accept high IDs and observed birds. */
    f.blocks=blocks;f.block_count=3;
    names[0].name.ptr="born__100";names[0].name.len=9;
    names[1].name.ptr="em2300_crow__23";names[1].name.len=15;
    names[2].name.ptr="born__10";names[2].name.len=8;
    blocks[0].bone_type=2;blocks[0].bone_no=100;
    blocks[1].bone_type=2;blocks[1].bone_no=23;
    blocks[2].bone_type=2;blocks[2].bone_no=10;
    e=re4_character_clip_coverage(&m,&f,&cov);assert(e==RE4_OK);
    assert(cov.linked_rotation_channels==3 && cov.name_mapped_bones==3);
    names[2].name.ptr="born__100";names[2].name.len=9;
    e=re4_character_clip_coverage(&m,&f,&cov);assert(e==RE4_OK);
    assert(cov.linked_rotation_channels==1 && cov.unmatched_rotation_channels==2);
    puts("RE4 coverage test: shared high-ID / bird matching OK");return 0;
}
