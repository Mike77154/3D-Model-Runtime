#include "hlfp_internal.h"

#define HLFP_MDL_HDR_SIZE 244
#define HLFP_MDL_SEQHDR_SIZE 76
#define HLFP_MDL_BONE_SIZE 112
#define HLFP_MDL_BONECONTROLLER_SIZE 24
#define HLFP_MDL_HITBOX_SIZE 32
#define HLFP_MDL_SEQGROUP_SIZE 104
#define HLFP_MDL_SEQDESC_SIZE 176
#define HLFP_MDL_EVENT_SIZE 76
#define HLFP_MDL_ATTACHMENT_SIZE 88
#define HLFP_MDL_TEXTURE_SIZE 80
#define HLFP_MDL_BODYPART_SIZE 76
#define HLFP_MDL_MODEL_SIZE 112
#define HLFP_MDL_MESH_SIZE 20
#define HLFP_MDL_TRIVERT_SIZE 8
#define HLFP_MDL_ANIMOFF_SIZE 12

static int hlfp__mdl_read_fixed_f32(const hlfp_u8 *p, hlfp_fixed *out) {
    return hlfp__float_bits_to_fixed(hlfp__read_u32le(p), out);
}

static int hlfp__mdl_read_vec3(const hlfp_u8 *p, hlfp_vec3 *out) {
    int rc;
    rc = hlfp__mdl_read_fixed_f32(p + 0, &out->x);
    if (rc != HLFP_OK) {
        return rc;
    }
    rc = hlfp__mdl_read_fixed_f32(p + 4, &out->y);
    if (rc != HLFP_OK) {
        return rc;
    }
    rc = hlfp__mdl_read_fixed_f32(p + 8, &out->z);
    if (rc != HLFP_OK) {
        return rc;
    }
    return HLFP_OK;
}

static int hlfp__mdl_validate_magic_version(hlfp_u32 id, int version, hlfp_u32 expected_id, hlfp_error *err, const char *what) {
    if (id != expected_id) {
        return hlfp__set_error(err, HLFP_ERR_BAD_MAGIC, 0, 0, what);
    }
    if (version != HLFP_MDL_VERSION) {
        return hlfp__set_errorf(err, HLFP_ERR_BAD_VERSION, 0, 0, "unexpected MDL version: ", version);
    }
    return HLFP_OK;
}

static int hlfp__mdl_parse_header(const hlfp_buffer *buf, hlfp_mdl_header *hdr, hlfp_error *err) {
    const hlfp_u8 *p;
    int rc;

    p = hlfp__checked_range(buf, 0, HLFP_MDL_HDR_SIZE, err, "MDL header");
    if (p == 0) {
        return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
    }

    hlfp__zero(hdr, sizeof(*hdr));
    hdr->id = hlfp__read_u32le(p + 0);
    hdr->version = hlfp__read_i32le(p + 4);
    hlfp__copy_cstr(hdr->name, 64, (const char *)(p + 8), 64);
    hdr->length = hlfp__read_i32le(p + 72);

    rc = hlfp__mdl_read_vec3(p + 76, &hdr->eyeposition);
    if (rc != HLFP_OK) {
        return hlfp__set_error(err, rc, 0, 0, "invalid MDL eye position");
    }
    rc = hlfp__mdl_read_vec3(p + 88, &hdr->min);
    if (rc != HLFP_OK) {
        return hlfp__set_error(err, rc, 0, 0, "invalid MDL hull min");
    }
    rc = hlfp__mdl_read_vec3(p + 100, &hdr->max);
    if (rc != HLFP_OK) {
        return hlfp__set_error(err, rc, 0, 0, "invalid MDL hull max");
    }
    rc = hlfp__mdl_read_vec3(p + 112, &hdr->bbmin);
    if (rc != HLFP_OK) {
        return hlfp__set_error(err, rc, 0, 0, "invalid MDL bbmin");
    }
    rc = hlfp__mdl_read_vec3(p + 124, &hdr->bbmax);
    if (rc != HLFP_OK) {
        return hlfp__set_error(err, rc, 0, 0, "invalid MDL bbmax");
    }

    hdr->flags = hlfp__read_i32le(p + 136);
    hdr->numbones = hlfp__read_i32le(p + 140);
    hdr->boneindex = hlfp__read_i32le(p + 144);
    hdr->numbonecontrollers = hlfp__read_i32le(p + 148);
    hdr->bonecontrollerindex = hlfp__read_i32le(p + 152);
    hdr->numhitboxes = hlfp__read_i32le(p + 156);
    hdr->hitboxindex = hlfp__read_i32le(p + 160);
    hdr->numseq = hlfp__read_i32le(p + 164);
    hdr->seqindex = hlfp__read_i32le(p + 168);
    hdr->numseqgroups = hlfp__read_i32le(p + 172);
    hdr->seqgroupindex = hlfp__read_i32le(p + 176);
    hdr->numtextures = hlfp__read_i32le(p + 180);
    hdr->textureindex = hlfp__read_i32le(p + 184);
    hdr->texturedataindex = hlfp__read_i32le(p + 188);
    hdr->numskinref = hlfp__read_i32le(p + 192);
    hdr->numskinfamilies = hlfp__read_i32le(p + 196);
    hdr->skinindex = hlfp__read_i32le(p + 200);
    hdr->numbodyparts = hlfp__read_i32le(p + 204);
    hdr->bodypartindex = hlfp__read_i32le(p + 208);
    hdr->numattachments = hlfp__read_i32le(p + 212);
    hdr->attachmentindex = hlfp__read_i32le(p + 216);
    hdr->soundtable = hlfp__read_i32le(p + 220);
    hdr->soundindex = hlfp__read_i32le(p + 224);
    hdr->soundgroups = hlfp__read_i32le(p + 228);
    hdr->soundgroupindex = hlfp__read_i32le(p + 232);
    hdr->numtransitions = hlfp__read_i32le(p + 236);
    hdr->transitionindex = hlfp__read_i32le(p + 240);

    rc = hlfp__mdl_validate_magic_version(hdr->id, hdr->version, HLFP_MDL_IDST, err, "expected IDST GoldSrc MDL header");
    if (rc != HLFP_OK) {
        return rc;
    }
    if (hdr->length <= 0 || (size_t)hdr->length > buf->size) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "invalid MDL length field");
    }
    return HLFP_OK;
}

static int hlfp__mdl_parse_seqhdr(const hlfp_buffer *buf, hlfp_u32 *id, int *version, char name[64], int *length, hlfp_error *err) {
    const hlfp_u8 *p;
    p = hlfp__checked_range(buf, 0, HLFP_MDL_SEQHDR_SIZE, err, "sequence group header");
    if (p == 0) {
        return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
    }
    if (id != 0) {
        *id = hlfp__read_u32le(p + 0);
    }
    if (version != 0) {
        *version = hlfp__read_i32le(p + 4);
    }
    if (name != 0) {
        hlfp__copy_cstr(name, 64, (const char *)(p + 8), 64);
    }
    if (length != 0) {
        *length = hlfp__read_i32le(p + 72);
    }
    return HLFP_OK;
}

static int hlfp__mdl_check_nonnegative_count(int count, int cap, hlfp_error *err, const char *what) {
    if (count < 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, what);
    }
    if (count > cap) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, what);
    }
    return HLFP_OK;
}

static int hlfp__mdl_parse_bones(const hlfp_buffer *buf, const hlfp_mdl_header *hdr, hlfp_mdl *out, hlfp_error *err) {
    int i;
    int j;
    int rc;
    const hlfp_u8 *p;
    hlfp_mdl_bone *dst;

    rc = hlfp__mdl_check_nonnegative_count(hdr->numbones, HLFP_MDL_MAX_BONES, err, "bone count exceeds fixed capacity");
    if (rc != HLFP_OK) {
        return rc;
    }
    out->num_bones = hdr->numbones;
    if (hdr->numbones == 0) {
        return HLFP_OK;
    }

    p = hlfp__checked_range(buf, (size_t)hdr->boneindex, (size_t)hdr->numbones * HLFP_MDL_BONE_SIZE, err, "bone table");
    if (p == 0) {
        return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
    }

    for (i = 0; i < hdr->numbones; ++i) {
        dst = &out->bones[i];
        hlfp__copy_cstr(dst->name, 32, (const char *)(p + i * HLFP_MDL_BONE_SIZE + 0), 32);
        dst->parent = hlfp__read_i32le(p + i * HLFP_MDL_BONE_SIZE + 32);
        dst->flags = hlfp__read_i32le(p + i * HLFP_MDL_BONE_SIZE + 36);
        for (j = 0; j < 6; ++j) {
            dst->bonecontroller[j] = hlfp__read_i32le(p + i * HLFP_MDL_BONE_SIZE + 40 + j * 4);
        }
        for (j = 0; j < 6; ++j) {
            rc = hlfp__mdl_read_fixed_f32(p + i * HLFP_MDL_BONE_SIZE + 64 + j * 4, &dst->value[j]);
            if (rc != HLFP_OK) {
                return hlfp__set_error(err, rc, 0, 0, "invalid bone default value");
            }
        }
        for (j = 0; j < 6; ++j) {
            rc = hlfp__mdl_read_fixed_f32(p + i * HLFP_MDL_BONE_SIZE + 88 + j * 4, &dst->scale[j]);
            if (rc != HLFP_OK) {
                return hlfp__set_error(err, rc, 0, 0, "invalid bone scale value");
            }
        }
    }
    return HLFP_OK;
}

static int hlfp__mdl_parse_bonecontrollers(const hlfp_buffer *buf, const hlfp_mdl_header *hdr, hlfp_mdl *out, hlfp_error *err) {
    int i;
    int rc;
    const hlfp_u8 *p;
    hlfp_mdl_bonecontroller *dst;

    rc = hlfp__mdl_check_nonnegative_count(hdr->numbonecontrollers, HLFP_MDL_MAX_BONE_CONTROLLERS, err, "bone controller count exceeds fixed capacity");
    if (rc != HLFP_OK) {
        return rc;
    }
    out->num_bonecontrollers = hdr->numbonecontrollers;
    if (hdr->numbonecontrollers == 0) {
        return HLFP_OK;
    }

    p = hlfp__checked_range(buf, (size_t)hdr->bonecontrollerindex, (size_t)hdr->numbonecontrollers * HLFP_MDL_BONECONTROLLER_SIZE, err, "bone controller table");
    if (p == 0) {
        return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
    }
    for (i = 0; i < hdr->numbonecontrollers; ++i) {
        dst = &out->bonecontrollers[i];
        dst->bone = hlfp__read_i32le(p + i * HLFP_MDL_BONECONTROLLER_SIZE + 0);
        dst->type = hlfp__read_i32le(p + i * HLFP_MDL_BONECONTROLLER_SIZE + 4);
        rc = hlfp__mdl_read_fixed_f32(p + i * HLFP_MDL_BONECONTROLLER_SIZE + 8, &dst->start);
        if (rc != HLFP_OK) {
            return hlfp__set_error(err, rc, 0, 0, "invalid bone controller start");
        }
        rc = hlfp__mdl_read_fixed_f32(p + i * HLFP_MDL_BONECONTROLLER_SIZE + 12, &dst->end);
        if (rc != HLFP_OK) {
            return hlfp__set_error(err, rc, 0, 0, "invalid bone controller end");
        }
        dst->rest = hlfp__read_i32le(p + i * HLFP_MDL_BONECONTROLLER_SIZE + 16);
        dst->index = hlfp__read_i32le(p + i * HLFP_MDL_BONECONTROLLER_SIZE + 20);
    }
    return HLFP_OK;
}

static int hlfp__mdl_parse_hitboxes(const hlfp_buffer *buf, const hlfp_mdl_header *hdr, hlfp_mdl *out, hlfp_error *err) {
    int i;
    int rc;
    const hlfp_u8 *p;
    hlfp_mdl_hitbox *dst;

    rc = hlfp__mdl_check_nonnegative_count(hdr->numhitboxes, HLFP_MDL_MAX_HITBOXES, err, "hitbox count exceeds fixed capacity");
    if (rc != HLFP_OK) {
        return rc;
    }
    out->num_hitboxes = hdr->numhitboxes;
    if (hdr->numhitboxes == 0) {
        return HLFP_OK;
    }

    p = hlfp__checked_range(buf, (size_t)hdr->hitboxindex, (size_t)hdr->numhitboxes * HLFP_MDL_HITBOX_SIZE, err, "hitbox table");
    if (p == 0) {
        return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
    }
    for (i = 0; i < hdr->numhitboxes; ++i) {
        dst = &out->hitboxes[i];
        dst->bone = hlfp__read_i32le(p + i * HLFP_MDL_HITBOX_SIZE + 0);
        dst->group = hlfp__read_i32le(p + i * HLFP_MDL_HITBOX_SIZE + 4);
        rc = hlfp__mdl_read_vec3(p + i * HLFP_MDL_HITBOX_SIZE + 8, &dst->bbmin);
        if (rc != HLFP_OK) {
            return hlfp__set_error(err, rc, 0, 0, "invalid hitbox bbmin");
        }
        rc = hlfp__mdl_read_vec3(p + i * HLFP_MDL_HITBOX_SIZE + 20, &dst->bbmax);
        if (rc != HLFP_OK) {
            return hlfp__set_error(err, rc, 0, 0, "invalid hitbox bbmax");
        }
    }
    return HLFP_OK;
}

static int hlfp__mdl_parse_seqgroups(const hlfp_buffer *buf, const hlfp_mdl_header *hdr, hlfp_mdl *out, hlfp_error *err) {
    int i;
    int rc;
    const hlfp_u8 *p;
    hlfp_mdl_seqgroup *dst;

    rc = hlfp__mdl_check_nonnegative_count(hdr->numseqgroups, HLFP_MDL_MAX_SEQGROUPS, err, "sequence group count exceeds fixed capacity");
    if (rc != HLFP_OK) {
        return rc;
    }
    out->num_seqgroups = hdr->numseqgroups;
    if (hdr->numseqgroups == 0) {
        return HLFP_OK;
    }

    p = hlfp__checked_range(buf, (size_t)hdr->seqgroupindex, (size_t)hdr->numseqgroups * HLFP_MDL_SEQGROUP_SIZE, err, "sequence group table");
    if (p == 0) {
        return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
    }
    for (i = 0; i < hdr->numseqgroups; ++i) {
        dst = &out->seqgroups[i];
        hlfp__copy_cstr(dst->label, 32, (const char *)(p + i * HLFP_MDL_SEQGROUP_SIZE + 0), 32);
        hlfp__copy_cstr(dst->name, 64, (const char *)(p + i * HLFP_MDL_SEQGROUP_SIZE + 32), 64);
        dst->cache = hlfp__read_i32le(p + i * HLFP_MDL_SEQGROUP_SIZE + 96);
        dst->data = hlfp__read_i32le(p + i * HLFP_MDL_SEQGROUP_SIZE + 100);
    }
    return HLFP_OK;
}

static int hlfp__mdl_parse_seqdesc(const hlfp_buffer *buf, const hlfp_mdl_header *hdr, hlfp_mdl *out, hlfp_error *err) {
    int i;
    int rc;
    const hlfp_u8 *p;
    hlfp_mdl_seqdesc *dst;

    rc = hlfp__mdl_check_nonnegative_count(hdr->numseq, HLFP_MDL_MAX_SEQUENCES, err, "sequence count exceeds fixed capacity");
    if (rc != HLFP_OK) {
        return rc;
    }
    out->num_seqdesc = hdr->numseq;
    if (hdr->numseq == 0) {
        return HLFP_OK;
    }

    p = hlfp__checked_range(buf, (size_t)hdr->seqindex, (size_t)hdr->numseq * HLFP_MDL_SEQDESC_SIZE, err, "sequence description table");
    if (p == 0) {
        return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
    }
    for (i = 0; i < hdr->numseq; ++i) {
        const hlfp_u8 *q;
        int j;
        q = p + i * HLFP_MDL_SEQDESC_SIZE;
        dst = &out->seqdesc[i];
        hlfp__copy_cstr(dst->label, 32, (const char *)(q + 0), 32);
        rc = hlfp__mdl_read_fixed_f32(q + 32, &dst->fps);
        if (rc != HLFP_OK) {
            return hlfp__set_error(err, rc, 0, 0, "invalid sequence fps");
        }
        dst->flags = hlfp__read_i32le(q + 36);
        dst->activity = hlfp__read_i32le(q + 40);
        dst->actweight = hlfp__read_i32le(q + 44);
        dst->numevents = hlfp__read_i32le(q + 48);
        dst->eventindex = hlfp__read_i32le(q + 52);
        dst->numframes = hlfp__read_i32le(q + 56);
        dst->numpivots = hlfp__read_i32le(q + 60);
        dst->pivotindex = hlfp__read_i32le(q + 64);
        dst->motiontype = hlfp__read_i32le(q + 68);
        dst->motionbone = hlfp__read_i32le(q + 72);
        rc = hlfp__mdl_read_vec3(q + 76, &dst->linearmovement);
        if (rc != HLFP_OK) {
            return hlfp__set_error(err, rc, 0, 0, "invalid sequence linearmovement");
        }
        dst->automoveposindex = hlfp__read_i32le(q + 88);
        dst->automoveangleindex = hlfp__read_i32le(q + 92);
        rc = hlfp__mdl_read_vec3(q + 96, &dst->bbmin);
        if (rc != HLFP_OK) {
            return hlfp__set_error(err, rc, 0, 0, "invalid sequence bbmin");
        }
        rc = hlfp__mdl_read_vec3(q + 108, &dst->bbmax);
        if (rc != HLFP_OK) {
            return hlfp__set_error(err, rc, 0, 0, "invalid sequence bbmax");
        }
        dst->numblends = hlfp__read_i32le(q + 120);
        dst->animindex = hlfp__read_i32le(q + 124);
        for (j = 0; j < 2; ++j) {
            dst->blendtype[j] = hlfp__read_i32le(q + 128 + j * 4);
        }
        for (j = 0; j < 2; ++j) {
            rc = hlfp__mdl_read_fixed_f32(q + 136 + j * 4, &dst->blendstart[j]);
            if (rc != HLFP_OK) {
                return hlfp__set_error(err, rc, 0, 0, "invalid sequence blendstart");
            }
        }
        for (j = 0; j < 2; ++j) {
            rc = hlfp__mdl_read_fixed_f32(q + 144 + j * 4, &dst->blendend[j]);
            if (rc != HLFP_OK) {
                return hlfp__set_error(err, rc, 0, 0, "invalid sequence blendend");
            }
        }
        dst->blendparent = hlfp__read_i32le(q + 152);
        dst->seqgroup = hlfp__read_i32le(q + 156);
        dst->entrynode = hlfp__read_i32le(q + 160);
        dst->exitnode = hlfp__read_i32le(q + 164);
        dst->nodeflags = hlfp__read_i32le(q + 168);
        dst->nextseq = hlfp__read_i32le(q + 172);
    }
    return HLFP_OK;
}

static int hlfp__mdl_parse_events(const hlfp_buffer *buf, hlfp_mdl *out, hlfp_error *err) {
    int i;
    int total;
    int j;
    const hlfp_u8 *p;
    hlfp_mdl_seqdesc *seq;
    hlfp_mdl_event *ev;

    total = 0;
    for (i = 0; i < out->num_seqdesc; ++i) {
        seq = &out->seqdesc[i];
        if (seq->numevents < 0) {
            return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "negative event count in sequence");
        }
        if (seq->numevents == 0) {
            continue;
        }
        if (total + seq->numevents > HLFP_MDL_MAX_EVENTS) {
            return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "event count exceeds fixed capacity");
        }
        p = hlfp__checked_range(buf, (size_t)seq->eventindex, (size_t)seq->numevents * HLFP_MDL_EVENT_SIZE, err, "sequence event table");
        if (p == 0) {
            return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
        }
        for (j = 0; j < seq->numevents; ++j) {
            ev = &out->events[total++];
            ev->frame = hlfp__read_i32le(p + j * HLFP_MDL_EVENT_SIZE + 0);
            ev->event = hlfp__read_i32le(p + j * HLFP_MDL_EVENT_SIZE + 4);
            ev->type = hlfp__read_i32le(p + j * HLFP_MDL_EVENT_SIZE + 8);
            hlfp__copy_cstr(ev->options, 64, (const char *)(p + j * HLFP_MDL_EVENT_SIZE + 12), 64);
        }
    }
    out->num_events = total;
    return HLFP_OK;
}

static int hlfp__mdl_parse_attachments(const hlfp_buffer *buf, const hlfp_mdl_header *hdr, hlfp_mdl *out, hlfp_error *err) {
    int i;
    int rc;
    const hlfp_u8 *p;
    hlfp_mdl_attachment *dst;

    rc = hlfp__mdl_check_nonnegative_count(hdr->numattachments, HLFP_MDL_MAX_ATTACHMENTS, err, "attachment count exceeds fixed capacity");
    if (rc != HLFP_OK) {
        return rc;
    }
    out->num_attachments = hdr->numattachments;
    if (hdr->numattachments == 0) {
        return HLFP_OK;
    }
    p = hlfp__checked_range(buf, (size_t)hdr->attachmentindex, (size_t)hdr->numattachments * HLFP_MDL_ATTACHMENT_SIZE, err, "attachment table");
    if (p == 0) {
        return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
    }
    for (i = 0; i < hdr->numattachments; ++i) {
        dst = &out->attachments[i];
        hlfp__copy_cstr(dst->name, 32, (const char *)(p + i * HLFP_MDL_ATTACHMENT_SIZE + 0), 32);
        dst->type = hlfp__read_i32le(p + i * HLFP_MDL_ATTACHMENT_SIZE + 32);
        dst->bone = hlfp__read_i32le(p + i * HLFP_MDL_ATTACHMENT_SIZE + 36);
        rc = hlfp__mdl_read_vec3(p + i * HLFP_MDL_ATTACHMENT_SIZE + 40, &dst->org);
        if (rc != HLFP_OK) {
            return hlfp__set_error(err, rc, 0, 0, "invalid attachment origin");
        }
        rc = hlfp__mdl_read_vec3(p + i * HLFP_MDL_ATTACHMENT_SIZE + 52, &dst->vectors[0]);
        if (rc != HLFP_OK) {
            return hlfp__set_error(err, rc, 0, 0, "invalid attachment axis 0");
        }
        rc = hlfp__mdl_read_vec3(p + i * HLFP_MDL_ATTACHMENT_SIZE + 64, &dst->vectors[1]);
        if (rc != HLFP_OK) {
            return hlfp__set_error(err, rc, 0, 0, "invalid attachment axis 1");
        }
        rc = hlfp__mdl_read_vec3(p + i * HLFP_MDL_ATTACHMENT_SIZE + 76, &dst->vectors[2]);
        if (rc != HLFP_OK) {
            return hlfp__set_error(err, rc, 0, 0, "invalid attachment axis 2");
        }
    }
    return HLFP_OK;
}

static int hlfp__mdl_parse_textures_and_skins(const hlfp_buffer *buf, const hlfp_mdl_header *hdr, hlfp_mdl *out, hlfp_error *err) {
    int i;
    int j;
    int rc;
    const hlfp_u8 *p;
    const hlfp_u8 *s;
    hlfp_mdl_texture *dst;

    rc = hlfp__mdl_check_nonnegative_count(hdr->numtextures, HLFP_MDL_MAX_TEXTURES, err, "texture count exceeds fixed capacity");
    if (rc != HLFP_OK) {
        return rc;
    }
    rc = hlfp__mdl_check_nonnegative_count(hdr->numskinref, HLFP_MDL_MAX_SKINREF, err, "skinref count exceeds fixed capacity");
    if (rc != HLFP_OK) {
        return rc;
    }
    rc = hlfp__mdl_check_nonnegative_count(hdr->numskinfamilies, HLFP_MDL_MAX_SKINFAMILIES, err, "skinfamily count exceeds fixed capacity");
    if (rc != HLFP_OK) {
        return rc;
    }

    out->num_textures = hdr->numtextures;
    out->num_skinref = hdr->numskinref;
    out->num_skinfamilies = hdr->numskinfamilies;

    if (hdr->numtextures > 0) {
        p = hlfp__checked_range(buf, (size_t)hdr->textureindex, (size_t)hdr->numtextures * HLFP_MDL_TEXTURE_SIZE, err, "texture table");
        if (p == 0) {
            return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
        }
        for (i = 0; i < hdr->numtextures; ++i) {
            dst = &out->textures[i];
            hlfp__copy_cstr(dst->name, 64, (const char *)(p + i * HLFP_MDL_TEXTURE_SIZE + 0), 64);
            dst->flags = hlfp__read_i32le(p + i * HLFP_MDL_TEXTURE_SIZE + 64);
            dst->width = hlfp__read_i32le(p + i * HLFP_MDL_TEXTURE_SIZE + 68);
            dst->height = hlfp__read_i32le(p + i * HLFP_MDL_TEXTURE_SIZE + 72);
            dst->index = hlfp__read_i32le(p + i * HLFP_MDL_TEXTURE_SIZE + 76);
        }
    }

    if (hdr->numskinref > 0 && hdr->numskinfamilies > 0) {
        s = hlfp__checked_range(buf, (size_t)hdr->skinindex, (size_t)hdr->numskinref * (size_t)hdr->numskinfamilies * 2U, err, "skinref table");
        if (s == 0) {
            return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
        }
        for (i = 0; i < hdr->numskinfamilies; ++i) {
            for (j = 0; j < hdr->numskinref; ++j) {
                out->skinrefs[i][j] = hlfp__read_i16le(s + ((size_t)i * (size_t)hdr->numskinref + (size_t)j) * 2U);
            }
        }
    }
    return HLFP_OK;
}

static int hlfp__mdl_parse_bodyparts_models_meshes(const hlfp_buffer *buf, const hlfp_mdl_header *hdr, hlfp_mdl *out, hlfp_error *err) {
    int i;
    int j;
    int k;
    int rc;
    const hlfp_u8 *bp;
    int total_models;
    int total_meshes;
    hlfp_mdl_bodypart *bodypart;
    hlfp_mdl_model *model;
    hlfp_mdl_mesh *mesh;
    const hlfp_u8 *pmodel;
    const hlfp_u8 *pmesh;

    rc = hlfp__mdl_check_nonnegative_count(hdr->numbodyparts, HLFP_MDL_MAX_BODYPARTS, err, "bodypart count exceeds fixed capacity");
    if (rc != HLFP_OK) {
        return rc;
    }
    out->num_bodyparts = hdr->numbodyparts;
    out->num_models = 0;
    out->num_meshes = 0;
    if (hdr->numbodyparts == 0) {
        return HLFP_OK;
    }

    bp = hlfp__checked_range(buf, (size_t)hdr->bodypartindex, (size_t)hdr->numbodyparts * HLFP_MDL_BODYPART_SIZE, err, "bodypart table");
    if (bp == 0) {
        return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
    }

    total_models = 0;
    total_meshes = 0;
    for (i = 0; i < hdr->numbodyparts; ++i) {
        const hlfp_u8 *q;
        q = bp + i * HLFP_MDL_BODYPART_SIZE;
        bodypart = &out->bodyparts[i];
        hlfp__copy_cstr(bodypart->name, 64, (const char *)(q + 0), 64);
        bodypart->nummodels = hlfp__read_i32le(q + 64);
        bodypart->base = hlfp__read_i32le(q + 68);
        bodypart->modelindex = hlfp__read_i32le(q + 72);
        bodypart->first_model = total_models;

        if (bodypart->nummodels < 0) {
            return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "negative model count in bodypart");
        }
        if (total_models + bodypart->nummodels > HLFP_MDL_MAX_MODELS) {
            return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "model count exceeds fixed capacity");
        }
        pmodel = hlfp__checked_range(buf, (size_t)bodypart->modelindex, (size_t)bodypart->nummodels * HLFP_MDL_MODEL_SIZE, err, "model table in bodypart");
        if (pmodel == 0 && bodypart->nummodels > 0) {
            return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
        }

        for (j = 0; j < bodypart->nummodels; ++j) {
            int model_index;
            const hlfp_u8 *m;
            model_index = total_models;
            model = &out->models[model_index];
            m = pmodel + j * HLFP_MDL_MODEL_SIZE;
            hlfp__copy_cstr(model->name, 64, (const char *)(m + 0), 64);
            model->type = hlfp__read_i32le(m + 64);
            rc = hlfp__mdl_read_fixed_f32(m + 68, &model->boundingradius);
            if (rc != HLFP_OK) {
                return hlfp__set_error(err, rc, 0, 0, "invalid model bounding radius");
            }
            model->nummesh = hlfp__read_i32le(m + 72);
            model->meshindex = hlfp__read_i32le(m + 76);
            model->numverts = hlfp__read_i32le(m + 80);
            model->vertinfoindex = hlfp__read_i32le(m + 84);
            model->vertindex = hlfp__read_i32le(m + 88);
            model->numnorms = hlfp__read_i32le(m + 92);
            model->norminfoindex = hlfp__read_i32le(m + 96);
            model->normindex = hlfp__read_i32le(m + 100);
            model->numgroups = hlfp__read_i32le(m + 104);
            model->groupindex = hlfp__read_i32le(m + 108);
            model->first_mesh = total_meshes;
            model->bodypart_index = i;

            if (model->nummesh < 0) {
                return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "negative mesh count in model");
            }
            if (model->numverts < 0 || model->numnorms < 0) {
                return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "negative vertex or normal count in model");
            }
            if (total_meshes + model->nummesh > HLFP_MDL_MAX_MESHES) {
                return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "mesh count exceeds fixed capacity");
            }
            pmesh = hlfp__checked_range(buf, (size_t)model->meshindex, (size_t)model->nummesh * HLFP_MDL_MESH_SIZE, err, "mesh table in model");
            if (pmesh == 0 && model->nummesh > 0) {
                return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
            }
            for (k = 0; k < model->nummesh; ++k) {
                mesh = &out->meshes[total_meshes];
                mesh->numtris = hlfp__read_i32le(pmesh + k * HLFP_MDL_MESH_SIZE + 0);
                mesh->triindex = hlfp__read_i32le(pmesh + k * HLFP_MDL_MESH_SIZE + 4);
                mesh->skinref = hlfp__read_i32le(pmesh + k * HLFP_MDL_MESH_SIZE + 8);
                mesh->numnorms = hlfp__read_i32le(pmesh + k * HLFP_MDL_MESH_SIZE + 12);
                mesh->normindex = hlfp__read_i32le(pmesh + k * HLFP_MDL_MESH_SIZE + 16);
                mesh->parent_model = model_index;
                ++total_meshes;
            }
            ++total_models;
        }
    }

    out->num_models = total_models;
    out->num_meshes = total_meshes;
    return HLFP_OK;
}

static int hlfp__mdl_parse_transitions(const hlfp_buffer *buf, const hlfp_mdl_header *hdr, hlfp_mdl *out, hlfp_error *err) {
    const hlfp_u8 *p;
    size_t count;
    int rc;

    rc = hlfp__mdl_check_nonnegative_count(hdr->numtransitions, HLFP_MDL_MAX_TRANSITIONS, err, "transition node count exceeds fixed capacity");
    if (rc != HLFP_OK) {
        return rc;
    }
    out->num_transitions = hdr->numtransitions;
    if (hdr->numtransitions == 0) {
        return HLFP_OK;
    }
    count = (size_t)hdr->numtransitions * (size_t)hdr->numtransitions;
    p = hlfp__checked_range(buf, (size_t)hdr->transitionindex, count, err, "transition matrix");
    if (p == 0) {
        return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
    }
    memcpy(out->transitions, p, count);
    return HLFP_OK;
}

static int hlfp__mdl_pick_texture_index(const hlfp_mdl *mdl, const hlfp_mdl_mesh *mesh, int *texture_index, hlfp_error *err) {
    int idx;
    if (mesh->skinref < 0 || mesh->skinref >= mdl->num_skinref) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "mesh skinref out of range");
    }
    if (mdl->num_skinfamilies > 0) {
        idx = mdl->skinrefs[0][mesh->skinref];
    } else {
        idx = mesh->skinref;
    }
    if (idx < 0 || idx >= mdl->num_textures) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "resolved texture index out of range");
    }
    *texture_index = idx;
    return HLFP_OK;
}

static int hlfp__mdl_read_model_vector_raw(const hlfp_mdl *mdl, const hlfp_mdl_model *model, int index, int is_normal, hlfp_vec3 *vec, int *bone, hlfp_error *err) {
    const hlfp_u8 *p;
    const hlfp_u8 *b;
    int count;
    int base_index;
    int info_index;
    int rc;

    if (mdl == 0 || model == 0 || vec == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null model vector request");
    }
    count = is_normal ? model->numnorms : model->numverts;
    base_index = is_normal ? model->normindex : model->vertindex;
    info_index = is_normal ? model->norminfoindex : model->vertinfoindex;
    if (index < 0 || index >= count) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "model vector index out of range");
    }
    p = hlfp__checked_range(&mdl->main_buffer, (size_t)base_index + (size_t)index * 12U, 12U, err, is_normal ? "model normal" : "model vertex");
    if (p == 0) {
        return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
    }
    rc = hlfp__mdl_read_vec3(p, vec);
    if (rc != HLFP_OK) {
        return hlfp__set_error(err, rc, 0, 0, is_normal ? "invalid normal vector" : "invalid vertex vector");
    }
    if (bone != 0) {
        b = hlfp__checked_range(&mdl->main_buffer, (size_t)info_index + (size_t)index, 1U, err, is_normal ? "normal bone info" : "vertex bone info");
        if (b == 0) {
            return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
        }
        *bone = (int)b[0];
    }
    return HLFP_OK;
}

static int hlfp__mdl_expand_add_vertex(const hlfp_mdl *mdl, const hlfp_mdl_model *model, const hlfp_mdl_mesh *mesh, int texture_index, const hlfp_mdl_trivert *tv, hlfp_mdl_mesh_expanded *out, int *out_index, hlfp_error *err) {
    int i;
    hlfp_mdl_expanded_vertex *v;
    int rc;
    hlfp_fixed tmp;

    for (i = 0; i < out->num_vertices; ++i) {
        v = &out->vertices[i];
        if (v->vertindex == tv->vertindex &&
            v->normindex == tv->normindex &&
            v->s == tv->s &&
            v->t == tv->t) {
            *out_index = i;
            return HLFP_OK;
        }
    }

    if (out->num_vertices >= HLFP_MDL_MAX_EXPANDED_VERTICES) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "expanded mesh vertex capacity exceeded");
    }

    v = &out->vertices[out->num_vertices];
    hlfp__zero(v, sizeof(*v));
    v->vertindex = tv->vertindex;
    v->normindex = tv->normindex;
    v->s = tv->s;
    v->t = tv->t;

    rc = hlfp__mdl_read_model_vector_raw(mdl, model, (int)tv->vertindex, 0, &v->pos, &v->bone, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    rc = hlfp__mdl_read_model_vector_raw(mdl, model, (int)tv->normindex, 1, &v->normal, &v->normal_bone, err);
    if (rc != HLFP_OK) {
        return rc;
    }

    if (mdl->textures[texture_index].width <= 0 || mdl->textures[texture_index].height <= 0) {
        return hlfp__set_error(err, HLFP_ERR_DIV_BY_ZERO, 0, 0, "texture dimensions must be positive");
    }

    tmp = (hlfp_fixed)tv->s * HLFP_FIXED_ONE;
    v->u = tmp / mdl->textures[texture_index].width;
    tmp = (hlfp_fixed)tv->t * HLFP_FIXED_ONE;
    v->v = tmp / mdl->textures[texture_index].height;

    *out_index = out->num_vertices;
    ++out->num_vertices;
    HLFP_UNUSED(mesh);
    return HLFP_OK;
}

static int hlfp__mdl_decode_anim_component(const hlfp_buffer *buf, const hlfp_u8 *anim_entry, hlfp_u16 offset, int frame, hlfp_fixed scale, hlfp_fixed *out, hlfp_error *err) {
    const hlfp_u8 *p;
    int k;
    int valid;
    int total;
    int sample_index;
    int rc;

    *out = 0;
    if (offset == 0) {
        return HLFP_OK;
    }

    p = hlfp__checked_range(buf, (size_t)(anim_entry - buf->data) + (size_t)offset, 2U, err, "animation value span");
    if (p == 0) {
        return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
    }

    k = frame;
    for (;;) {
        p = hlfp__checked_range(buf, (size_t)(p - buf->data), 2U, err, "animation value span");
        if (p == 0) {
            return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
        }
        valid = (int)p[0];
        total = (int)p[1];
        if (total <= 0) {
            return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "animation span has non-positive total");
        }
        if (k < total) {
            if (valid > k) {
                sample_index = k + 1;
            } else {
                sample_index = valid;
            }
            p = hlfp__checked_range(buf, (size_t)(p - buf->data) + (size_t)sample_index * 2U, 2U, err, "animation sample");
            if (p == 0) {
                return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
            }
            rc = hlfp__fixed_mul_i16(scale, hlfp__read_i16le(p), out);
            if (rc != HLFP_OK) {
                return hlfp__set_error(err, rc, 0, 0, "fixed-point overflow in animation sample");
            }
            return HLFP_OK;
        }
        k -= total;
        p = hlfp__checked_range(buf, (size_t)(p - buf->data) + (size_t)(valid + 1) * 2U, 2U, err, "next animation span");
        if (p == 0) {
            return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
        }
    }
}

void hlfp_mdl_init(hlfp_mdl *out) {
    if (out != 0) {
        hlfp__zero(out, sizeof(*out));
    }
}

int hlfp_mdl_parse_main(const void *data, size_t size, hlfp_mdl *out, hlfp_error *err) {
    int rc;
    hlfp_buffer buf;

    if (data == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null MDL input or output");
    }

    hlfp_error_clear(err);
    hlfp_mdl_init(out);
    buf.data = (const hlfp_u8 *)data;
    buf.size = size;
    out->main_buffer = buf;

    rc = hlfp__mdl_parse_header(&buf, &out->header, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    rc = hlfp__mdl_parse_bones(&buf, &out->header, out, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    rc = hlfp__mdl_parse_bonecontrollers(&buf, &out->header, out, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    rc = hlfp__mdl_parse_hitboxes(&buf, &out->header, out, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    rc = hlfp__mdl_parse_seqgroups(&buf, &out->header, out, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    rc = hlfp__mdl_parse_seqdesc(&buf, &out->header, out, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    rc = hlfp__mdl_parse_events(&buf, out, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    rc = hlfp__mdl_parse_attachments(&buf, &out->header, out, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    rc = hlfp__mdl_parse_textures_and_skins(&buf, &out->header, out, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    rc = hlfp__mdl_parse_bodyparts_models_meshes(&buf, &out->header, out, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    rc = hlfp__mdl_parse_transitions(&buf, &out->header, out, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    return HLFP_OK;
}

int hlfp_mdl_attach_texture_file(hlfp_mdl *out, const void *data, size_t size, hlfp_error *err) {
    hlfp_buffer buf;
    hlfp_mdl_header hdr;
    int rc;

    if (out == 0 || data == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null texture MDL input or output");
    }
    hlfp_error_clear(err);

    buf.data = (const hlfp_u8 *)data;
    buf.size = size;
    rc = hlfp__mdl_parse_header(&buf, &hdr, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    if (hdr.numtextures <= 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "texture MDL has no textures");
    }
    out->texture_buffer = buf;
    rc = hlfp__mdl_parse_textures_and_skins(&buf, &hdr, out, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    return HLFP_OK;
}

int hlfp_mdl_attach_seqgroup_file(hlfp_mdl *out, int seqgroup_index, const void *data, size_t size, hlfp_error *err) {
    hlfp_buffer buf;
    hlfp_u32 id = 0;
    int version = 0;
    char name[64];
    int length = 0;
    int rc;

    if (out == 0 || data == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null sequence group input or output");
    }
    if (seqgroup_index <= 0 || seqgroup_index >= HLFP_MDL_MAX_SEQGROUPS) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "sequence group index out of range");
    }
    if (out->num_seqgroups > 0 && seqgroup_index >= out->num_seqgroups) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "sequence group index exceeds parsed table");
    }
    hlfp_error_clear(err);

    buf.data = (const hlfp_u8 *)data;
    buf.size = size;
    rc = hlfp__mdl_parse_seqhdr(&buf, &id, &version, name, &length, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    HLFP_UNUSED(name);
    if (id != HLFP_MDL_IDSQ) {
        return hlfp__set_error(err, HLFP_ERR_BAD_MAGIC, 0, 0, "expected IDSQ sequence group header");
    }
    if (version != HLFP_MDL_VERSION) {
        return hlfp__set_errorf(err, HLFP_ERR_BAD_VERSION, 0, 0, "unexpected sequence group version: ", version);
    }
    if (length <= 0 || (size_t)length > buf.size) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "invalid sequence group length");
    }
    out->seqgroup_buffers[seqgroup_index] = buf;
    return HLFP_OK;
}

int hlfp_mdl_get_texture_pixels(const hlfp_mdl *mdl, int texture_index,
                                const hlfp_u8 **pixels, const hlfp_u8 **palette,
                                int *width, int *height, hlfp_error *err) {
    const hlfp_buffer *buf;
    const hlfp_mdl_texture *tex;
    const hlfp_u8 *p;
    size_t bytes;

    if (mdl == 0 || pixels == 0 || palette == 0 || width == 0 || height == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null texture query argument");
    }
    hlfp_error_clear(err);
    if (texture_index < 0 || texture_index >= mdl->num_textures) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "texture index out of range");
    }
    buf = (mdl->texture_buffer.data != 0) ? &mdl->texture_buffer : &mdl->main_buffer;
    if (buf->data == 0 || buf->size == 0) {
        return hlfp__set_error(err, HLFP_ERR_MISSING_DATA, 0, 0, "no texture source buffer attached");
    }
    tex = &mdl->textures[texture_index];
    if (tex->width <= 0 || tex->height <= 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "texture dimensions must be positive");
    }
    bytes = (size_t)tex->width * (size_t)tex->height;
    p = hlfp__checked_range(buf, (size_t)tex->index, bytes + 768U, err, "texture pixel block");
    if (p == 0) {
        return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
    }
    *pixels = p;
    *palette = p + bytes;
    *width = tex->width;
    *height = tex->height;
    return HLFP_OK;
}

int hlfp_mdl_read_model_vertex(const hlfp_mdl *mdl, int model_index, int vertex_index,
                               hlfp_vec3 *pos, int *bone, hlfp_error *err) {
    if (mdl == 0 || pos == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null model vertex request");
    }
    hlfp_error_clear(err);
    if (model_index < 0 || model_index >= mdl->num_models) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "model index out of range");
    }
    return hlfp__mdl_read_model_vector_raw(mdl, &mdl->models[model_index], vertex_index, 0, pos, bone, err);
}

int hlfp_mdl_read_model_normal(const hlfp_mdl *mdl, int model_index, int normal_index,
                               hlfp_vec3 *normal, int *bone, hlfp_error *err) {
    if (mdl == 0 || normal == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null model normal request");
    }
    hlfp_error_clear(err);
    if (model_index < 0 || model_index >= mdl->num_models) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "model index out of range");
    }
    return hlfp__mdl_read_model_vector_raw(mdl, &mdl->models[model_index], normal_index, 1, normal, bone, err);
}

int hlfp_mdl_expand_mesh(const hlfp_mdl *mdl, int model_index, int mesh_index,
                         hlfp_mdl_mesh_expanded *out, hlfp_error *err) {
    const hlfp_mdl_model *model;
    const hlfp_mdl_mesh *mesh;
    int rc;
    int texture_index = 0;
    size_t pos;
    int cmd;
    int count;
    int is_fan;
    int i;
    int face;
    int local_indices[HLFP_MDL_MAX_TRICMD_VERTICES];
    hlfp_mdl_trivert tv;
    const hlfp_u8 *p;

    if (mdl == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null mesh expansion request");
    }
    hlfp_error_clear(err);
    hlfp__zero(out, sizeof(*out));

    if (model_index < 0 || model_index >= mdl->num_models) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "model index out of range");
    }
    if (mesh_index < 0 || mesh_index >= mdl->num_meshes) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "mesh index out of range");
    }
    model = &mdl->models[model_index];
    mesh = &mdl->meshes[mesh_index];
    if (mesh->parent_model != model_index) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "mesh does not belong to requested model");
    }

    rc = hlfp__mdl_pick_texture_index(mdl, mesh, &texture_index, err);
    if (rc != HLFP_OK) {
        return rc;
    }

    out->model_index = model_index;
    out->mesh_index = mesh_index;
    out->texture_index = texture_index;

    pos = (size_t)mesh->triindex;
    for (;;) {
        p = hlfp__checked_range(&mdl->main_buffer, pos, 2U, err, "triangle command");
        if (p == 0) {
            return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
        }
        cmd = (int)hlfp__read_i16le(p);
        pos += 2U;
        if (cmd == 0) {
            break;
        }
        is_fan = 0;
        count = cmd;
        if (count < 0) {
            is_fan = 1;
            count = -count;
        }
        if (count > HLFP_MDL_MAX_TRICMD_VERTICES) {
            return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "triangle command vertex count exceeds fixed capacity");
        }
        for (i = 0; i < count; ++i) {
            p = hlfp__checked_range(&mdl->main_buffer, pos, HLFP_MDL_TRIVERT_SIZE, err, "triangle command vertex");
            if (p == 0) {
                return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
            }
            tv.vertindex = hlfp__read_i16le(p + 0);
            tv.normindex = hlfp__read_i16le(p + 2);
            tv.s = hlfp__read_i16le(p + 4);
            tv.t = hlfp__read_i16le(p + 6);
            pos += HLFP_MDL_TRIVERT_SIZE;
            if (tv.vertindex < 0 || tv.vertindex >= model->numverts || tv.normindex < 0 || tv.normindex >= model->numnorms) {
                return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "trivert references vertex or normal outside model range");
            }
            rc = hlfp__mdl_expand_add_vertex(mdl, model, mesh, texture_index, &tv, out, &local_indices[i], err);
            if (rc != HLFP_OK) {
                return rc;
            }
        }
        if (count >= 3) {
            for (face = 0; face < count - 2; ++face) {
                if (out->num_triangles >= HLFP_MDL_MAX_EXPANDED_TRIANGLES) {
                    return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "expanded mesh triangle capacity exceeded");
                }
                if (is_fan) {
                    out->triangles[out->num_triangles].v0 = local_indices[0];
                    out->triangles[out->num_triangles].v1 = local_indices[face + 1];
                    out->triangles[out->num_triangles].v2 = local_indices[face + 2];
                } else if (face & 1) {
                    out->triangles[out->num_triangles].v0 = local_indices[face + 1];
                    out->triangles[out->num_triangles].v1 = local_indices[face];
                    out->triangles[out->num_triangles].v2 = local_indices[face + 2];
                } else {
                    out->triangles[out->num_triangles].v0 = local_indices[face];
                    out->triangles[out->num_triangles].v1 = local_indices[face + 1];
                    out->triangles[out->num_triangles].v2 = local_indices[face + 2];
                }
                ++out->num_triangles;
            }
        }
    }

    return HLFP_OK;
}

int hlfp_mdl_decode_sequence_frame(const hlfp_mdl *mdl, int sequence_index,
                                   int blend_index, int frame,
                                   hlfp_mdl_pose_set *out, hlfp_error *err) {
    const hlfp_mdl_seqdesc *seq;
    const hlfp_mdl_seqgroup *group;
    hlfp_buffer buf;
    size_t base_off;
    size_t entry_off;
    int bone;
    int rc;
    const hlfp_u8 *entry;
    hlfp_fixed delta;

    if (mdl == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null sequence decode request");
    }
    hlfp_error_clear(err);
    hlfp__zero(out, sizeof(*out));

    if (sequence_index < 0 || sequence_index >= mdl->num_seqdesc) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "sequence index out of range");
    }
    seq = &mdl->seqdesc[sequence_index];
    if (seq->seqgroup < 0 || seq->seqgroup >= mdl->num_seqgroups) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "sequence group index out of range");
    }
    if (seq->numframes <= 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "sequence has no frames");
    }
    if (frame < 0 || frame >= seq->numframes) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "frame index out of range");
    }
    if (seq->numblends <= 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "sequence has no blends");
    }
    if (blend_index < 0 || blend_index >= seq->numblends) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "blend index out of range");
    }

    group = &mdl->seqgroups[seq->seqgroup];
    if (seq->seqgroup == 0) {
        buf = mdl->main_buffer;
        base_off = (size_t)group->data + (size_t)seq->animindex;
    } else {
        buf = mdl->seqgroup_buffers[seq->seqgroup];
        if (buf.data == 0 || buf.size == 0) {
            return hlfp__set_error(err, HLFP_ERR_MISSING_DATA, 0, 0, "external sequence group buffer not attached");
        }
        base_off = (size_t)seq->animindex;
    }

    for (bone = 0; bone < mdl->num_bones; ++bone) {
        hlfp_pose *pose;
        entry_off = base_off + ((size_t)blend_index * (size_t)mdl->num_bones + (size_t)bone) * HLFP_MDL_ANIMOFF_SIZE;
        entry = hlfp__checked_range(&buf, entry_off, HLFP_MDL_ANIMOFF_SIZE, err, "animation offset entry");
        if (entry == 0) {
            return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
        }
        pose = &out->poses[bone];
        pose->bone = bone;
        pose->pos.x = mdl->bones[bone].value[0];
        pose->pos.y = mdl->bones[bone].value[1];
        pose->pos.z = mdl->bones[bone].value[2];
        pose->rot.x = mdl->bones[bone].value[3];
        pose->rot.y = mdl->bones[bone].value[4];
        pose->rot.z = mdl->bones[bone].value[5];
        rc = hlfp__mdl_decode_anim_component(&buf, entry, hlfp__read_u16le(entry + 0), frame, mdl->bones[bone].scale[0], &delta, err);
        if (rc != HLFP_OK) {
            return rc;
        }
        pose->pos.x += delta;
        rc = hlfp__mdl_decode_anim_component(&buf, entry, hlfp__read_u16le(entry + 2), frame, mdl->bones[bone].scale[1], &delta, err);
        if (rc != HLFP_OK) {
            return rc;
        }
        pose->pos.y += delta;
        rc = hlfp__mdl_decode_anim_component(&buf, entry, hlfp__read_u16le(entry + 4), frame, mdl->bones[bone].scale[2], &delta, err);
        if (rc != HLFP_OK) {
            return rc;
        }
        pose->pos.z += delta;
        rc = hlfp__mdl_decode_anim_component(&buf, entry, hlfp__read_u16le(entry + 6), frame, mdl->bones[bone].scale[3], &delta, err);
        if (rc != HLFP_OK) {
            return rc;
        }
        pose->rot.x += delta;
        rc = hlfp__mdl_decode_anim_component(&buf, entry, hlfp__read_u16le(entry + 8), frame, mdl->bones[bone].scale[4], &delta, err);
        if (rc != HLFP_OK) {
            return rc;
        }
        pose->rot.y += delta;
        rc = hlfp__mdl_decode_anim_component(&buf, entry, hlfp__read_u16le(entry + 10), frame, mdl->bones[bone].scale[5], &delta, err);
        if (rc != HLFP_OK) {
            return rc;
        }
        pose->rot.z += delta;
    }

    out->num_bones = mdl->num_bones;
    return HLFP_OK;
}
