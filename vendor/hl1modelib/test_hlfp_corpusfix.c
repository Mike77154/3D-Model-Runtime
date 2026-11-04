#include "hlfp_bundle.h"
#include <stdio.h>
#include <string.h>

static hlfp_smd_model model;
static hlfp_error err;

int main(void) {
    static const char sample[] =
        "version 1\n"
        "nodes\n0 \"root\" -1\nend\n"
        "skeleton\ntime 0\n0 0 0 0 0 0 0\nend\n"
        "triangles\n"
        "\n"
        "0 0 0 0 0 0 1 0 0 7 0 0.30 0 0.20 0 0.15 0 0.12 0 0.10 0 0.08 0 0.05\n"
        "0 1 0 0 0 0 1 1 0 7 0 0.30 0 0.20 0 0.15 0 0.12 0 0.10 0 0.08 0 0.05\n"
        "0 0 1 0 0 0 1 0 1 7 0 0.30 0 0.20 0 0.15 0 0.12 0 0.10 0 0.08 0 0.05\n"
        "end\n";
    int rc;
    char name[100];
    hlfp_smd_init(&model);
    rc = hlfp_smd_parse_buffer(sample, sizeof(sample)-1U, &model, &err);
    if (rc != HLFP_OK) {
        printf("corpusfix_parse_fail=%d %s\n",rc,err.message);
        return 1;
    }
    if (model.num_triangles != 1 ||
        strcmp(model.triangles[0].material,"unnamed_material") != 0 ||
        model.triangles[0].v[0].num_links != HLFP_SMD_MAX_LINKS ||
        model.discarded_weight_links != 3) {
        puts("corpusfix_weight_or_material_regression");
        return 1;
    }
    rc = hlfp_goldsrc_make_texture_filename("models/Mr.X",0,name,sizeof(name),&err);
    if (rc != HLFP_OK || strcmp(name,"models/Mr.XT.mdl") != 0) return 1;
    rc = hlfp_goldsrc_make_seqgroup_filename("models/zombie",1,0,name,sizeof(name),&err);
    if (rc != HLFP_OK || strcmp(name,"models/zombie01.mdl") != 0) return 1;
    puts("corpusfix_ok=1 unnamed_material=1 capped_weight_influences=6 discarded=3 companions=1");
    return 0;
}
