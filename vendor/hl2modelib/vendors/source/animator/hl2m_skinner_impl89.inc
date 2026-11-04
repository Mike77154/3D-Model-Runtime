/* Vendored Source format submodule; included in legacy core to share C89 private parsers.
   No duplicated decoder definitions. See docs/MODULAR_V1_0_4.md. */
int hl2m_engine_cpu_skin(const hl2m_engine_model *base_model, const hl2m_pose_matrices *inverse_bind, const hl2m_pose_matrices *pose_world, hl2m_engine_model *out_model, hl2m_skin_report *out_report)
{
    int i;
    hl2m_skin_report local_report;
    if (base_model == 0 || inverse_bind == 0 || pose_world == 0 || out_model == 0) return HL2M_ERR_NULL;
    *out_model = *base_model;
    hl2m_clear_skin_report(&local_report);
    local_report.vertex_count = base_model->vertex_count;
    for (i = 0; i < out_model->vertex_count; ++i) {
        const hl2m_engine_vertex *src;
        hl2m_vec3 pos_acc;
        hl2m_vec3 norm_acc;
        hl2m_fixed total_weight;
        int w;
        int used;
        src = &base_model->vertices[i];
        pos_acc.x = pos_acc.y = pos_acc.z = 0;
        norm_acc.x = norm_acc.y = norm_acc.z = 0;
        total_weight = 0;
        used = 0;
        if (src->bone_count <= 0) {
            ++local_report.unweighted_count;
            local_report.warning_flags |= HL2M_SKIN_WARN_NO_WEIGHTS;
            continue;
        }
        for (w = 0; w < src->bone_count && w < HL2M_MAX_BONES_PER_VERT; ++w) {
            int b;
            hl2m_fixed wt;
            hl2m_vec3 p0;
            hl2m_vec3 p1;
            hl2m_vec3 n1;
            b = src->bone[w];
            wt = src->weight[w];
            if (wt == 0) continue;
            if (b < 0 || b >= inverse_bind->bone_count || b >= pose_world->bone_count) {
                ++local_report.missing_bone_count;
                local_report.warning_flags |= HL2M_SKIN_WARN_BAD_BONE;
                continue;
            }
            p0 = hl2m_mat_transform_point(&inverse_bind->world[b], src->position);
            p1 = hl2m_mat_transform_point(&pose_world->world[b], p0);
            /* Normal requires inverse bind rotation before posed rotation,
               exactly as position does (but without translation). */
            n1 = hl2m_mat_transform_vector(&inverse_bind->world[b], src->normal);
            n1 = hl2m_mat_transform_vector(&pose_world->world[b], n1);
            pos_acc.x += hl2m_fixed_mul_unit_approx(p1.x, wt);
            pos_acc.y += hl2m_fixed_mul_unit_approx(p1.y, wt);
            pos_acc.z += hl2m_fixed_mul_unit_approx(p1.z, wt);
            norm_acc.x += hl2m_fixed_mul_unit_approx(n1.x, wt);
            norm_acc.y += hl2m_fixed_mul_unit_approx(n1.y, wt);
            norm_acc.z += hl2m_fixed_mul_unit_approx(n1.z, wt);
            total_weight += wt;
            used = 1;
        }
        if (!used) {
            out_model->vertices[i].position = src->position;
            out_model->vertices[i].normal = src->normal;
            ++local_report.unweighted_count;
        } else {
            if (total_weight < HL2M_FIXED_ONE) {
                hl2m_fixed left;
                left = HL2M_FIXED_ONE - total_weight;
                pos_acc.x += hl2m_fixed_mul_unit_approx(src->position.x, left);
                pos_acc.y += hl2m_fixed_mul_unit_approx(src->position.y, left);
                pos_acc.z += hl2m_fixed_mul_unit_approx(src->position.z, left);
                norm_acc.x += hl2m_fixed_mul_unit_approx(src->normal.x, left);
                norm_acc.y += hl2m_fixed_mul_unit_approx(src->normal.y, left);
                norm_acc.z += hl2m_fixed_mul_unit_approx(src->normal.z, left);
                ++local_report.partial_weight_count;
                local_report.warning_flags |= HL2M_SKIN_WARN_PARTIAL_WEIGHT;
            }
            out_model->vertices[i].position = pos_acc;
            out_model->vertices[i].normal = norm_acc;
            ++local_report.skinned_count;
        }
    }
    if (out_report != 0) *out_report = local_report;
    return HL2M_OK;
}

