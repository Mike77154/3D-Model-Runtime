#include "c4d89.h"
#include <string.h>
/* This is deliberately evidence, not a hierarchy/skin solver. The SDK serializes
   the complete object graph with references not established by this decoder. */
static int skip_name(const char *s)
{
    return strcmp(s,"Joint")==0 || strcmp(s,"Weight")==0 ||
      strcmp(s,"Skin")==0 || strcmp(s,"Polygon")==0 ||
      strcmp(s,"Shadow")==0 || strcmp(s,"Phong")==0 ||
      strcmp(s,"Texture")==0 || strcmp(s,"UVW")==0 ||
      strcmp(s,"SceneHook")==0 || strcmp(s,"Polygon Selection")==0 ||
      strcmp(s,"Tracks")==0 || strcmp(s,"Position")==0 ||
      strcmp(s,"Rotation")==0 || strcmp(s,"Scale")==0 ||
      strstr(s,"Position . ")!=0 || strstr(s,"Rotation . ")!=0 ||
      strstr(s,"Scale . ")!=0 || strstr(s,"Selection ")==s;
}
int c4d89_rig_evidence(const C4D89_Report *r,C4D89_RigEvidence *out)
{
    unsigned int i;unsigned long start=0ul,end=0ul;
    if(!r||!out)return C4D89_ERR_CORRUPT;
    memset(out,0,sizeof(*out));
    for(i=0u;i<r->name_count;i++) {
        const char *s=r->names[i].text;
        if(strcmp(s,"Joint")==0) {out->joint_type_strings++;if(start==0ul)start=r->names[i].offset;}
        if(strcmp(s,"Skin")==0)out->skin_type_strings++;
        if(strcmp(s,"Weight")==0)out->weight_type_strings++;
        if(strcmp(s,"SceneHook")==0 && end==0ul)end=r->names[i].offset;
    }
    if(end==0ul)end=r->file_size;
    if(start==0ul||end<=start)return C4D89_OK;
    for(i=0u;i<r->name_count;i++) {
        const C4D89_Name *n=&r->names[i];
        if(n->offset<=start||n->offset>=end||n->is_texture_reference||skip_name(n->text))continue;
        /* A typed name found in the joint-containing portion of the scene.
           Candidate only; not proven to be a joint or parent relation. */
        if(out->candidate_joint_count<C4D89_MAX_JOINT_HINTS) {
            C4D89_JointHint *h=&out->joint_hints[out->candidate_joint_count++];
            memcpy(h->name,n->text,C4D89_MAX_NAME);h->name_offset=n->offset;
        } else out->omitted_joint_hints++;
    }
    return C4D89_OK;
}
