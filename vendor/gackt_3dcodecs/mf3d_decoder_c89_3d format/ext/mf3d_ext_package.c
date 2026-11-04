#include "mf3d_ext_package.h"
#include "mf3d_ext_xml.h"

static void scopy(char *dst, int cap, const char *src)
{
    int i;
    if (!dst || cap <= 0) return;
    dst[0] = 0;
    if (!src) return;
    for (i = 0; i < cap - 1 && src[i]; ++i) dst[i] = src[i];
    dst[i] = 0;
}

static int has_token(const char *s, const char *tok)
{
    int i, n;
    if (!s || !tok) return 0;
    n = 0; while (tok[n]) n++;
    while (*s) {
        while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
        i = 0;
        while (tok[i] && s[i] == tok[i]) i++;
        if (i == n && (s[i] == 0 || s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) return 1;
        while (*s && *s != ' ' && *s != '\t' && *s != '\r' && *s != '\n') s++;
    }
    return 0;
}

static mf3d_u32 cap_from_token(const char *t)
{
    if (!t) return 0;
    if (has_token(t, "m") || has_token(t, "mat") || has_token(t, "materials")) return MF3D_CAP_MATERIALS;
    if (has_token(t, "p") || has_token(t, "prod") || has_token(t, "production")) return MF3D_CAP_PRODUCTION;
    if (has_token(t, "s") || has_token(t, "slice")) return MF3D_CAP_SLICE;
    if (has_token(t, "b") || has_token(t, "beam") || has_token(t, "beamlattice")) return MF3D_CAP_BEAMLATTICE;
    if (has_token(t, "sc") || has_token(t, "secure") || has_token(t, "securecontent")) return MF3D_CAP_SECURECONTENT;
    if (has_token(t, "bo") || has_token(t, "boolean") || has_token(t, "booleanoperations")) return MF3D_CAP_BOOLEAN;
    if (has_token(t, "d") || has_token(t, "disp") || has_token(t, "displacement")) return MF3D_CAP_DISPLACEMENT;
    if (has_token(t, "v") || has_token(t, "vol") || has_token(t, "volumetric")) return MF3D_CAP_VOLUMETRIC;
    if (has_token(t, "i") || has_token(t, "implicit")) return MF3D_CAP_IMPLICIT;
    return 0;
}

void mf3d_package_report_init(mf3d_package_report *r)
{
    if (!r) return;
    r->required_count = 0;
    r->present_caps = MF3D_CAP_CORE;
    r->unsupported_required_count = 0;
    r->warning_count = 0;
}

int mf3d_package_add_warning(mf3d_package_report *r, const char *msg)
{
    if (!r || !msg) return MF3D_E_BADARG;
    if (r->warning_count >= MF3D_EXT_MAX_WARNINGS) return MF3D_E_NOSPACE;
    scopy(r->warnings[r->warning_count], 96, msg);
    r->warning_count++;
    return MF3D_OK;
}

static void detect_presence(char *xml, mf3d_package_report *r)
{
    if (mf3d_ext_next_tag(xml, "basematerials") || mf3d_ext_next_tag(xml, "colorgroup") || mf3d_ext_next_tag(xml, "texture2d") || mf3d_ext_next_tag(xml, "multiproperties")) r->present_caps |= MF3D_CAP_MATERIALS;
    if (mf3d_ext_next_tag(xml, "slicestack")) r->present_caps |= MF3D_CAP_SLICE;
    if (mf3d_ext_next_tag(xml, "beamlattice")) r->present_caps |= MF3D_CAP_BEAMLATTICE;
    if (mf3d_ext_next_tag(xml, "booleanshape")) r->present_caps |= MF3D_CAP_BOOLEAN;
    if (mf3d_ext_next_tag(xml, "displacementmesh") || mf3d_ext_next_tag(xml, "displacement2d")) r->present_caps |= MF3D_CAP_DISPLACEMENT;
    if (mf3d_ext_next_tag(xml, "levelset") || mf3d_ext_next_tag(xml, "image3d") || mf3d_ext_next_tag(xml, "volumetricdata")) r->present_caps |= MF3D_CAP_VOLUMETRIC;
    if (mf3d_ext_next_tag(xml, "implicitfunction")) r->present_caps |= MF3D_CAP_IMPLICIT;
    if (mf3d_ext_next_tag(xml, "keystore") || mf3d_ext_next_tag(xml, "resourcedata")) r->present_caps |= MF3D_CAP_SECURECONTENT;
    if (has_token(xml, "path=\"") || has_token(xml, "path='")) r->present_caps |= MF3D_CAP_PRODUCTION;
}

int mf3d_package_scan_model_xml(char *xml, mf3d_package_report *r)
{
    const char *model;
    char req[512];
    int i, j;
    if (!xml || !r) return MF3D_E_BADARG;
    mf3d_package_report_init(r);
    detect_presence(xml, r);
    model = mf3d_ext_next_tag(xml, "model");
    if (!model) return MF3D_E_XML;
    mf3d_ext_attr(model, "requiredextensions", req, (int)sizeof(req));
    i = 0;
    while (req[i]) {
        while (req[i] == ' ' || req[i] == '\t' || req[i] == '\r' || req[i] == '\n') i++;
        if (!req[i]) break;
        if (r->required_count >= MF3D_EXT_MAX_REQUIRED) return MF3D_E_NOSPACE;
        j = 0;
        while (req[i] && req[i] != ' ' && req[i] != '\t' && req[i] != '\r' && req[i] != '\n' && j < 23) r->required[r->required_count][j++] = req[i++];
        r->required[r->required_count][j] = 0;
        if (cap_from_token(r->required[r->required_count]) == 0) r->unsupported_required_count++;
        r->required_count++;
    }
    return MF3D_OK;
}

int mf3d_package_required_supported(const mf3d_package_report *r)
{
    return r ? (r->unsupported_required_count == 0) : 0;
}

const char *mf3d_package_cap_name(mf3d_u32 cap)
{
    switch (cap) {
    case MF3D_CAP_CORE: return "core";
    case MF3D_CAP_MATERIALS: return "materials";
    case MF3D_CAP_PRODUCTION: return "production";
    case MF3D_CAP_SLICE: return "slice";
    case MF3D_CAP_BEAMLATTICE: return "beam_lattice";
    case MF3D_CAP_SECURECONTENT: return "secure_content";
    case MF3D_CAP_BOOLEAN: return "boolean_operations";
    case MF3D_CAP_DISPLACEMENT: return "displacement";
    case MF3D_CAP_VOLUMETRIC: return "volumetric";
    case MF3D_CAP_IMPLICIT: return "implicit";
    default: return "unknown";
    }
}
