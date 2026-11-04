#include "c4d89.h"
#include <stdio.h>
static C4D89_MeshTable meshes;
static C4D89_PolyRegions regions;
int main(int argc,char **argv)
{
    unsigned int i, m;
    int rc;
    if(argc!=2){fputs("Usage: c4d89_poly_regions FILE.c4d\n",stderr);return 2;}
    rc=c4d89_find_meshes(argv[1],&meshes);
    if(rc!=C4D89_OK){fprintf(stderr,"mesh probe error %d\n",rc);return 1;}
    rc=c4d89_scan_poly_regions(argv[1],&meshes,&regions);
    if(rc!=C4D89_OK){fprintf(stderr,"selection scan error %d\n",rc);return 1;}
    printf("C4D6 selections: %u valid ranges; %u omitted\n",regions.count,regions.omitted);
    for(m=0u;m<meshes.count;m++){
        unsigned long covered=0ul, overlaps=0ul, missing=0ul;
        unsigned long face;
        printf("Mesh %u: %lu faces\n",m,meshes.meshes[m].face_count);
        for(i=0u;i<regions.count;i++){
            const C4D89_PolyRange *r=&regions.ranges[i];
            if(r->mesh_index!=m)continue;
            printf("  %s: %lu..%lu (byte 0x%lX)\n",
                r->selection,r->first_face,r->last_face,r->marker_offset);
        }
        for(face=0ul;face<meshes.meshes[m].face_count;face++){
            unsigned int n=c4d89_find_face_regions(&regions,m,face,0,0u);
            if(n==0u)missing++;
            else {covered++;if(n>1u)overlaps++;}
        }
        printf("  Covered %lu / %lu; missing %lu; overlapping %lu\n",
            covered,meshes.meshes[m].face_count,missing,overlaps);
    }
    return 0;
}
