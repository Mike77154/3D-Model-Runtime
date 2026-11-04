/* Sample Source 2013 seqdesc weights/autolayers and pose on real MDL. */
#include "hl2m_master89.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define BUF119 (25u*1024u*1024u)
static hl2m_u8 bytes[BUF119];
static hl2m_mdl mdl;
static hl2m_seq_metadata89 meta;
static hl2m_pose_matrices raw,layered;
static hl2m_sequence_layers_report89 report;
int main(int argc,char **argv)
{
 FILE *f;long n;int seq,r,i,changed=0,nonzero=0;
 if(argc<3){fprintf(stderr,"Usage: %s model.mdl sequence_index\n",argv[0]);return 2;}
 f=fopen(argv[1],"rb");if(!f){perror("open");return 3;}
 if(fseek(f,0,SEEK_END)!=0){fclose(f);return 4;}
 n=ftell(f);if(n<=0 || (unsigned long)n>BUF119){fclose(f);return 5;}
 rewind(f);if(fread(bytes,1,(size_t)n,f)!=(size_t)n){fclose(f);return 6;}fclose(f);
 r=hl2m_decode_mdl(bytes,(unsigned int)n,&mdl);
 if(r){printf("mdl decode %d\n",r);return 7;}
 seq=atoi(argv[2]);
 if(seq<0 || seq>=mdl.sequence_count)return 8;
 r=hl2m_master_sequence_metadata89(bytes,(unsigned int)n,&mdl,seq,&meta);
 if(r){printf("meta error %d\n",r);return 9;}
 for(i=0;i<meta.bone_count;i++)if(meta.bone_weights[i]!=HL2M_FIXED_ONE)nonzero++;
 printf("MODEL %s SEQ %d %s FLAGS %x LAYERS %d WEIGHTS_CHANGED %d POSEKEYS %d\n",argv[1],seq,
     mdl.sequences[seq].label,meta.flags,meta.layer_count,nonzero,meta.has_posekeys);
 for(i=0;i<meta.layer_count;i++)printf(" LAYER %d -> sequence %d flags %x\n",i,
     meta.layers[i].sequence,meta.layers[i].flags);
 r=hl2m_master_sequence_pose89(bytes,(unsigned int)n,NULL,0u,&mdl,seq,
     HL2M_FIXED_ONE/2,0,0,&raw,NULL);
 printf("base sequence sample %d\n",r);
 if(r)return 10;
 r=hl2m_master_sequence_layered89(bytes,(unsigned int)n,NULL,0u,&mdl,seq,
     HL2M_FIXED_ONE/2,0,0,&layered,&report);
 printf("layer sample %d declared %d applied %d skipped %d delta %d unsupported %d\n",
     r,report.declared,report.applied,report.skipped_zero_weight,
     report.child_delta,report.unsupported_layer);
 if(r)return 11;
 for(i=0;i<raw.bone_count;i++){
    int row,col,diff=0;
    for(row=0;row<3;row++)for(col=0;col<4;col++)
        if(raw.local[i].m[row][col]-layered.local[i].m[row][col]>64 || layered.local[i].m[row][col]-raw.local[i].m[row][col]>64)diff=1;
    changed+=diff;
 }
 printf("CHANGED BONES (difference >64 Q16 units) %d/%d\n",changed,raw.bone_count);
 return 0;
}
