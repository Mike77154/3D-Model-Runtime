/* VTA flex decoder / writer using shared core text helpers. */
int hl2m_decode_vta(const hl2m_u8 *data, unsigned int size, hl2m_vta *out_vta)
{
    hl2m_stream s;
    char line[HL2M_LINE_MAX];
    const char *p;
    int current_flex;
    int time_value;
    int r;

    if (data == 0 || out_vta == 0) return HL2M_ERR_NULL;
    hl2m_clear_vta(out_vta);
    s.data = data;
    s.size = size;
    s.pos = 0u;
    current_flex = -1;
    while (hl2m_stream_line(&s, line, sizeof(line))) {
        p = hl2m_skip_spaces(line);
        if (*p == 0) continue;
        if (hl2m_text_eq_ci(p, "end")) {
            current_flex = -1;
            continue;
        }
        if (hl2m_starts_ci_v05(p, "version")) {
            p += 7;
            (void)hl2m_parse_int_ptr(&p, &out_vta->version);
            continue;
        }
        if (hl2m_starts_ci_v05(p, "time")) {
            p += 4;
            time_value = 0;
            r = hl2m_parse_int_ptr(&p, &time_value);
            if (r != HL2M_OK) return r;
            if (out_vta->flex_count >= HL2M_MAX_VTA_FLEXES) return HL2M_ERR_LIMIT;
            current_flex = out_vta->flex_count;
            out_vta->flexes[current_flex].time = time_value;
            out_vta->flexes[current_flex].first_vertex = out_vta->vertex_count;
            hl2m_copy_token(out_vta->flexes[current_flex].name, HL2M_NAME_MAX, "flex");
            out_vta->flexes[current_flex].name[4] = '_';
            out_vta->flexes[current_flex].name[5] = (char)('0' + ((time_value / 100) % 10));
            out_vta->flexes[current_flex].name[6] = (char)('0' + ((time_value / 10) % 10));
            out_vta->flexes[current_flex].name[7] = (char)('0' + (time_value % 10));
            out_vta->flexes[current_flex].name[8] = 0;
            ++out_vta->flex_count;
            continue;
        }
        if (*p >= '0' && *p <= '9' && current_flex >= 0) {
            int vertex_index;
            int vi;
            hl2m_vta_vertex *v;
            if (out_vta->vertex_count >= HL2M_MAX_VTA_VERTICES) return HL2M_ERR_LIMIT;
            r = hl2m_parse_int_ptr(&p, &vertex_index);
            if (r != HL2M_OK) return r;
            vi = out_vta->vertex_count;
            v = &out_vta->vertices[vi];
            v->flex = current_flex;
            v->source_vertex = vertex_index;
            r = hl2m_parse_fixed_ptr(&p, &v->position.x); if (r != HL2M_OK) return r;
            r = hl2m_parse_fixed_ptr(&p, &v->position.y); if (r != HL2M_OK) return r;
            r = hl2m_parse_fixed_ptr(&p, &v->position.z); if (r != HL2M_OK) return r;
            r = hl2m_parse_fixed_ptr(&p, &v->normal.x); if (r != HL2M_OK) return r;
            r = hl2m_parse_fixed_ptr(&p, &v->normal.y); if (r != HL2M_OK) return r;
            r = hl2m_parse_fixed_ptr(&p, &v->normal.z); if (r != HL2M_OK) return r;
            ++out_vta->vertex_count;
            out_vta->flexes[current_flex].num_vertices = out_vta->vertex_count - out_vta->flexes[current_flex].first_vertex;
        }
    }
    return HL2M_OK;
}

int hl2m_vta_write(const hl2m_vta *vta, char *out_text, unsigned int out_size, unsigned int *out_used)
{
    unsigned int used;
    int f;
    int i;
    int r;
    if (vta == 0 || out_text == 0 || out_used == 0) return HL2M_ERR_NULL;
    used = 0u;
    r = hl2m_out_append_text(out_text, out_size, &used, "version "); if (r != HL2M_OK) return r;
    r = hl2m_out_append_int(out_text, out_size, &used, vta->version != 0 ? vta->version : 1); if (r != HL2M_OK) return r;
    r = hl2m_out_append_text(out_text, out_size, &used, "\n"); if (r != HL2M_OK) return r;
    for (f = 0; f < vta->flex_count; ++f) {
        r = hl2m_out_append_text(out_text, out_size, &used, "time "); if (r != HL2M_OK) return r;
        r = hl2m_out_append_int(out_text, out_size, &used, vta->flexes[f].time); if (r != HL2M_OK) return r;
        r = hl2m_out_append_text(out_text, out_size, &used, "\n"); if (r != HL2M_OK) return r;
        for (i = 0; i < vta->flexes[f].num_vertices; ++i) {
            int vi;
            vi = vta->flexes[f].first_vertex + i;
            if (vi < 0 || vi >= vta->vertex_count) continue;
            r = hl2m_out_append_int(out_text, out_size, &used, vta->vertices[vi].source_vertex); if (r != HL2M_OK) return r;
            r = hl2m_out_append_text(out_text, out_size, &used, " "); if (r != HL2M_OK) return r;
            r = hl2m_append_fixed_qc(out_text, out_size, &used, vta->vertices[vi].position.x); if (r != HL2M_OK) return r;
            r = hl2m_out_append_text(out_text, out_size, &used, " "); if (r != HL2M_OK) return r;
            r = hl2m_append_fixed_qc(out_text, out_size, &used, vta->vertices[vi].position.y); if (r != HL2M_OK) return r;
            r = hl2m_out_append_text(out_text, out_size, &used, " "); if (r != HL2M_OK) return r;
            r = hl2m_append_fixed_qc(out_text, out_size, &used, vta->vertices[vi].position.z); if (r != HL2M_OK) return r;
            r = hl2m_out_append_text(out_text, out_size, &used, " "); if (r != HL2M_OK) return r;
            r = hl2m_append_fixed_qc(out_text, out_size, &used, vta->vertices[vi].normal.x); if (r != HL2M_OK) return r;
            r = hl2m_out_append_text(out_text, out_size, &used, " "); if (r != HL2M_OK) return r;
            r = hl2m_append_fixed_qc(out_text, out_size, &used, vta->vertices[vi].normal.y); if (r != HL2M_OK) return r;
            r = hl2m_out_append_text(out_text, out_size, &used, " "); if (r != HL2M_OK) return r;
            r = hl2m_append_fixed_qc(out_text, out_size, &used, vta->vertices[vi].normal.z); if (r != HL2M_OK) return r;
            r = hl2m_out_append_text(out_text, out_size, &used, "\n"); if (r != HL2M_OK) return r;
        }
        r = hl2m_out_append_text(out_text, out_size, &used, "end\n"); if (r != HL2M_OK) return r;
    }
    *out_used = used;
    return HL2M_OK;
}

