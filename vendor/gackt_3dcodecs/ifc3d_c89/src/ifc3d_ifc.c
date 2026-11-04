#include "ifc3d_internal.h"

static void entity_parse_refs_from_arg(ifc3d_entity *e, int arg_index)
{
    const char *a;
    int n;
    int count;
    if (ifc3d_arg_get(e->args, e->args_len, arg_index, &a, &n) != IFC3D_OK) return;
    if (ifc3d_arg_parse_ref_list(a, n, e->refs, IFC3D_ENTITY_MAX_REFS, &count) == IFC3D_OK) e->ref_count = count;
}

static void entity_parse_ref_arg(ifc3d_entity *e, int arg_index, long *dst)
{
    const char *a;
    int n;
    long r;
    *dst = 0;
    if (ifc3d_arg_get(e->args, e->args_len, arg_index, &a, &n) != IFC3D_OK) return;
    if (ifc3d_arg_is_omitted(a, n)) return;
    if (ifc3d_arg_parse_ref(a, n, &r) == IFC3D_OK) *dst = r;
}

static void entity_parse_fixed_arg(ifc3d_entity *e, int arg_index, ifc3d_fixed *dst)
{
    const char *a;
    int n;
    ifc3d_fixed v;
    *dst = 0;
    if (ifc3d_arg_get(e->args, e->args_len, arg_index, &a, &n) != IFC3D_OK) return;
    if (ifc3d_arg_is_omitted(a, n)) return;
    if (ifc3d_parse_fixed(a, n, &v) == IFC3D_OK) *dst = v;
}

ifc3d_status ifc3d_build_semantics(ifc3d_model *m, ifc3d_diag *diag)
{
    int i;
    const char *a;
    int n;
    ifc3d_entity *e;
    if (!m) return IFC3D_ERR_BAD_ARG;

    for (i = 0; i < m->entity_count; ++i) {
        e = &m->entities[i];
        e->ref_count = 0;
        e->ref0 = e->ref1 = e->ref2 = e->ref3 = 0;
        e->first = e->count = e->first2 = e->count2 = 0;
        e->flags = 0;
        e->scalar0 = e->scalar1 = e->scalar2 = 0;
        e->v.x = e->v.y = e->v.z = 0;
        e->v2.x = e->v2.y = e->v2.z = 0;

        switch (e->type) {
        case IFC3D_T_IFCCARTESIANPOINT:
        case IFC3D_T_IFCDIRECTION:
            if (ifc3d_arg_get(e->args, e->args_len, 0, &a, &n) == IFC3D_OK) {
                if (ifc3d_arg_parse_fixed_list3(a, n, &e->v) == IFC3D_OK) ++m->parsed_geometry_count;
            }
            break;
        case IFC3D_T_IFCAXIS2PLACEMENT3D:
            entity_parse_ref_arg(e, 0, &e->ref0); /* Location */
            entity_parse_ref_arg(e, 1, &e->ref1); /* Axis */
            entity_parse_ref_arg(e, 2, &e->ref2); /* RefDirection */
            ++m->parsed_geometry_count;
            break;
        case IFC3D_T_IFCAXIS2PLACEMENT2D:
            entity_parse_ref_arg(e, 0, &e->ref0);
            entity_parse_ref_arg(e, 1, &e->ref2);
            ++m->parsed_geometry_count;
            break;
        case IFC3D_T_IFCLOCALPLACEMENT:
            entity_parse_ref_arg(e, 0, &e->ref0); /* PlacementRelTo */
            entity_parse_ref_arg(e, 1, &e->ref1); /* RelativePlacement */
            ++m->parsed_geometry_count;
            break;
        case IFC3D_T_IFCPOLYLINE:
        case IFC3D_T_IFCPOLYLOOP:
        case IFC3D_T_IFCFACE:
        case IFC3D_T_IFCCLOSEDSHELL:
            entity_parse_refs_from_arg(e, 0);
            ++m->parsed_geometry_count;
            break;
        case IFC3D_T_IFCFACEOUTERBOUND:
        case IFC3D_T_IFCFACEBOUND:
            entity_parse_ref_arg(e, 0, &e->ref0); /* bound loop */
            if (ifc3d_arg_get(e->args, e->args_len, 1, &a, &n) == IFC3D_OK) e->flags = (unsigned int)ifc3d_arg_parse_bool(a, n, 1);
            else e->flags = 1U;
            ++m->parsed_geometry_count;
            break;
        case IFC3D_T_IFCFACETEDBREP:
            entity_parse_ref_arg(e, 0, &e->ref0); /* Outer shell */
            ++m->parsed_geometry_count;
            break;
        case IFC3D_T_IFCCARTESIANPOINTLIST3D:
            if (ifc3d_arg_get(e->args, e->args_len, 0, &a, &n) == IFC3D_OK) {
                if (ifc3d_parse_nested_point_list3(m, e, a, n, diag) != IFC3D_OK) return IFC3D_ERR_OVERFLOW;
                ++m->parsed_geometry_count;
            }
            break;
        case IFC3D_T_IFCTRIANGULATEDFACESET:
            entity_parse_ref_arg(e, 0, &e->ref0); /* Coordinates */
            if (ifc3d_arg_get(e->args, e->args_len, 3, &a, &n) == IFC3D_OK) {
                if (ifc3d_parse_nested_tri_indices(m, e, a, n, diag) != IFC3D_OK) return IFC3D_ERR_OVERFLOW;
                ++m->parsed_geometry_count;
            }
            break;
        case IFC3D_T_IFCARBITRARYCLOSEDPROFILEDEF:
            entity_parse_ref_arg(e, 2, &e->ref0); /* OuterCurve */
            ++m->parsed_geometry_count;
            break;
        case IFC3D_T_IFCRECTANGLEPROFILEDEF:
            entity_parse_ref_arg(e, 2, &e->ref0); /* Position */
            entity_parse_fixed_arg(e, 3, &e->scalar0); /* XDim */
            entity_parse_fixed_arg(e, 4, &e->scalar1); /* YDim */
            ++m->parsed_geometry_count;
            break;
        case IFC3D_T_IFCCIRCLEPROFILEDEF:
            entity_parse_ref_arg(e, 2, &e->ref0); /* Position */
            entity_parse_fixed_arg(e, 3, &e->scalar0); /* Radius */
            ++m->parsed_geometry_count;
            break;
        case IFC3D_T_IFCEXTRUDEDAREASOLID:
            entity_parse_ref_arg(e, 0, &e->ref0); /* SweptArea */
            entity_parse_ref_arg(e, 1, &e->ref1); /* Position */
            entity_parse_ref_arg(e, 2, &e->ref2); /* ExtrudedDirection */
            entity_parse_fixed_arg(e, 3, &e->scalar0); /* Depth */
            ++m->parsed_geometry_count;
            break;
        case IFC3D_T_IFCSHAPEREPRESENTATION:
            entity_parse_refs_from_arg(e, 3); /* Items */
            ++m->parsed_geometry_count;
            break;
        case IFC3D_T_IFCPRODUCTDEFINITIONSHAPE:
            entity_parse_refs_from_arg(e, 2); /* Representations */
            ++m->parsed_geometry_count;
            break;
        default:
            break;
        }
    }
    ifc3d_set_diag(diag, IFC3D_OK, 0, "ok");
    return IFC3D_OK;
}
