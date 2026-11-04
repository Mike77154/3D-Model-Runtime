#include "c4d89.h"
#include <string.h>
/* Candidate grouping: nearest earlier typed texture path in the native scene.
   The record preserves uncertainty: it is NOT a polygon/material mapping. */
int c4d89_texture_hints(const C4D89_Report *r,C4D89_TextureCatalog *c)
{
    unsigned int i,j,k,ix,best;
    unsigned long bestoff,area,oldarea;
    if(!r||!c)return C4D89_ERR_CORRUPT;
    memset(c,0,sizeof(*c));
    for(i=0u;i<r->name_count;i++) {
        if(!r->names[i].is_texture_reference)continue;
        if(c->count>=C4D89_MAX_TEXTURES)return C4D89_ERR_LIMIT;
        k=c->count++;
        memcpy(c->textures[k].path,r->names[i].text,C4D89_MAX_NAME);
        c->textures[k].string_offset=r->names[i].offset;
        c->textures[k].best_jpeg_index=C4D89_MAX_IMAGES;
    }
    for(i=0u;i<r->image_count;i++) {
        ix=C4D89_MAX_TEXTURES;bestoff=0ul;
        for(j=0u;j<c->count;j++) {
            if(c->textures[j].string_offset<r->images[i].offset &&
              (ix==C4D89_MAX_TEXTURES || c->textures[j].string_offset>bestoff)) {
                ix=j;bestoff=c->textures[j].string_offset;
            }
        }
        if(ix==C4D89_MAX_TEXTURES){c->unassigned_jpegs++;continue;}
        c->textures[ix].jpeg_count++;
        best=c->textures[ix].best_jpeg_index;
        area=(unsigned long)r->images[i].width*(unsigned long)r->images[i].height;
        oldarea=best==C4D89_MAX_IMAGES?0ul:
           (unsigned long)r->images[best].width*(unsigned long)r->images[best].height;
        if(best==C4D89_MAX_IMAGES || area>oldarea ||
          (area==oldarea && r->images[i].components>r->images[best].components)) {
            c->textures[ix].best_jpeg_index=i;
            c->textures[ix].best_width=r->images[i].width;
            c->textures[ix].best_height=r->images[i].height;
        }
    }
    return C4D89_OK;
}
