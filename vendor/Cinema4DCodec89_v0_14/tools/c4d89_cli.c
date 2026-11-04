#include "c4d89.h"
#include <stdio.h>
#include <string.h>

static C4D89_MeshTable meshes;
static C4D89_PolyRegions regions;
static C4D89_TextureTagLinks links;
static C4D89_Report report;  /* Avoid oversized stack use on Win32. */
static void usage(void)
{
    puts("Cinema4DCodec89 v0.9 - native mesh and polygon selection decoder");
    puts("Usage:");
    puts("  c4d89 info <model.c4d>");
    puts("  c4d89 names <model.c4d>");
    puts("  c4d89 images <model.c4d> <output_prefix>");
    puts("  c4d89 mesh <model.c4d>        (print verified native mesh groups)");
    puts("  c4d89 obj <model.c4d> <preview.obj> (debug export only)");
    puts("  c4d89 groups <model.c4d> <grouped.obj> (native polygon selection groups)");
    puts("  c4d89 smooth <model.c4d> <smooth.obj> (C89 crease normals)");
    puts("  c4d89 bindings <model.c4d> (TextureTag->Selection restriction)");
    puts("  c4d89 probe <model.c4d>");
}
int main(int argc,char **argv)
{
    int status; unsigned int i;
    if(argc<3) {usage();return 2;}
    if(strcmp(argv[1],"probe")==0) {
        int code=c4d89_probe(argv[2]);
        printf("Native signature: %s\n",code==1?"XC4DC4D6":code==2?"QC4DC4D6":"unrecognized");
        return code?0:1;
    }
    if(strcmp(argv[1],"mesh")==0) {
        status=c4d89_find_meshes(argv[2],&meshes);
        if(status){fprintf(stderr,"Mesh: %s\n",c4d89_error(status));return 1;}
        printf("Native meshes: %u\n",meshes.count);
        for(i=0u;i<meshes.count;i++)
            printf("%u: %lu vertices, %lu triangles, %lu quads, %lu UV corners\n",
                i,meshes.meshes[i].vertex_count,meshes.meshes[i].triangle_count,
                meshes.meshes[i].quad_count,meshes.meshes[i].uv_count);
        return 0;
    }
    if(strcmp(argv[1],"bindings")==0) {
        status=c4d89_find_meshes(argv[2],&meshes);
        if(status)return 1;
        status=c4d89_scan_poly_regions(argv[2],&meshes,&regions);
        if(status)return 1;
        status=c4d89_scan_texturetag_links(argv[2],&regions,&links);
        if(status)return 1;
        printf("TextureTags: %u; resolved restriction names: %u; unknown material pointers: %u\n",
               links.count,links.resolved_restrictions,links.count);
        for(i=0u;i<links.count;i++){
            printf("tag@0x%08lX -> ",links.tags[i].tag_offset);
            if(links.tags[i].verified_restriction)
                printf("Selection '%s' (face %lu..%lu, mesh %u)\n",
                       links.tags[i].restriction,
                       regions.ranges[links.tags[i].region_index].first_face,
                       regions.ranges[links.tags[i].region_index].last_face,
                       regions.ranges[links.tags[i].region_index].mesh_index);
            else puts("restriction not resolved");
        }
        return 0;
    }
    if(strcmp(argv[1],"smooth")==0) {
        if(argc<4){usage();return 2;}
        status=c4d89_export_obj_smooth(argv[2],argv[3],587);
        if(status)fprintf(stderr,"Smooth OBJ: %s\n",c4d89_error(status));
        else printf("Crease-aware OBJ: %s\n",argv[3]);
        return status?1:0;
    }
    if(strcmp(argv[1],"obj")==0 || strcmp(argv[1],"groups")==0) {
        if(argc<4){usage();return 2;}
        status=strcmp(argv[1],"groups")==0?c4d89_export_obj_groups(argv[2],argv[3]):
               c4d89_export_obj(argv[2],argv[3]);
        if(status)fprintf(stderr,"OBJ: %s\n",c4d89_error(status));
        else printf("Diagnostic OBJ exported: %s\n",argv[3]);
        return status?1:0;
    }
    if(strcmp(argv[1],"info") && strcmp(argv[1],"names") && strcmp(argv[1],"images")) {
        usage();return 2;
    }
    if(strcmp(argv[1],"images")==0 && argc<4) {usage();return 2;}
    status=c4d89_scan(argv[2],&report);
    if(status) {fprintf(stderr,"Error: %s\n",c4d89_error(status));return 1;}
    if(strcmp(argv[1],"info")==0) {
        printf("Signature: %s\nSize: %lu bytes\n",report.signature==1u?"XC4DC4D6":"QC4DC4D6",report.file_size);
        printf("Embedded JPEG streams: %u\nUnique typed strings: %u\n",report.image_count,report.name_count);
        printf("Omitted image entries: %u; omitted strings: %u\n",report.images_over_limit,report.names_over_limit);
        status=c4d89_find_meshes(argv[2],&meshes);
        if(status==C4D89_OK) {
            printf("Geometry: %u verified native mesh groups\n",meshes.count);
            for(i=0u;i<meshes.count;i++)
                printf("Mesh[%u] verts=%lu triangles=%lu quads=%lu uv=%lu\n",
                   i,meshes.meshes[i].vertex_count,meshes.meshes[i].triangle_count,
                   meshes.meshes[i].quad_count,meshes.meshes[i].uv_count);
        } else puts("Geometry: no verified decodable mesh group");
        for(i=0u;i<report.image_count;i++)
            printf("JPEG[%03u] offset=%lu length=%lu %ux%u channels=%u\n",i,
                report.images[i].offset,report.images[i].length,
                report.images[i].width,report.images[i].height,
                report.images[i].components);
        for(i=0u;i<report.name_count;i++)
            if(report.names[i].is_texture_reference)
                printf("Texture ref: %s\n",report.names[i].text);
    } else if(strcmp(argv[1],"names")==0) {
        for(i=0u;i<report.name_count;i++) printf("%8lu  %s%s\n",report.names[i].offset,
            report.names[i].is_texture_reference?"[texture] ":"",report.names[i].text);
    } else {
        if(strlen(argv[3])>420u) {fputs("Output prefix too long\n",stderr);return 2;}
        for(i=0u;i<report.image_count;i++) {
            char dest[512];
            sprintf(dest,"%s_%03u.jpg",argv[3],i);
            status=c4d89_extract_image(argv[2],&report.images[i],dest);
            if(status) {fprintf(stderr,"Image %u: %s\n",i,c4d89_error(status));return 1;}
            printf("%s (%ux%u)\n",dest,report.images[i].width,report.images[i].height);
        }
    }
    return 0;
}
