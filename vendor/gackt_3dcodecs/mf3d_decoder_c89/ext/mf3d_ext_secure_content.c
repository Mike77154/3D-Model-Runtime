#include "mf3d_ext_secure_content.h"
#include "mf3d_ext_xml.h"

extern int mf3d_parse_u32(const char *s, mf3d_u32 *out);

static void scopy(char *dst, int cap, const char *src)
{
    int i;
    if (!dst || cap <= 0) return;
    dst[0] = 0;
    if (!src) return;
    for (i = 0; i < cap - 1 && src[i]; ++i) dst[i] = src[i];
    dst[i] = 0;
}

static int streq(const char *a, const char *b)
{
    if (!a || !b) return 0;
    while (*a && *b) { if (*a != *b) return 0; ++a; ++b; }
    return *a == 0 && *b == 0;
}

static int b64v(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return 26 + c - 'a';
    if (c >= '0' && c <= '9') return 52 + c - '0';
    if (c == '+') return 62;
    if (c == '/') return 63;
    if (c == '=') return -2;
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') return -3;
    return -1;
}

int mf3d_secure_content_base64_decode(const char *src, mf3d_u8 *dst, mf3d_u32 dst_cap, mf3d_u32 *dst_len)
{
    int q[4], n, v;
    mf3d_u32 out;
    if (!src || !dst) return MF3D_E_BADARG;
    out = 0; n = 0;
    while (*src) {
        v = b64v(*src++);
        if (v == -3) continue;
        if (v == -1) return MF3D_E_RANGE;
        q[n++] = v;
        if (n == 4) {
            if (q[0] < 0 || q[1] < 0) return MF3D_E_RANGE;
            if (out >= dst_cap) return MF3D_E_NOSPACE;
            dst[out++] = (mf3d_u8)((q[0] << 2) | (q[1] >> 4));
            if (q[2] != -2) {
                if (q[2] < 0) return MF3D_E_RANGE;
                if (out >= dst_cap) return MF3D_E_NOSPACE;
                dst[out++] = (mf3d_u8)(((q[1] & 15) << 4) | (q[2] >> 2));
                if (q[3] != -2) {
                    if (q[3] < 0) return MF3D_E_RANGE;
                    if (out >= dst_cap) return MF3D_E_NOSPACE;
                    dst[out++] = (mf3d_u8)(((q[2] & 3) << 6) | q[3]);
                }
            }
            n = 0;
        }
    }
    if (n != 0) return MF3D_E_RANGE;
    if (dst_len) *dst_len = out;
    return MF3D_OK;
}

void mf3d_secure_content_init(mf3d_secure_content *s)
{
    if (!s) return;
    s->uuid[0] = 0;
    s->consumer_count = s->group_count = s->resource_count = s->right_count = 0;
}

static int parse_b64_child(const char *parent, const char *parent_end, const char *tag,
                           mf3d_u8 *dst, mf3d_u32 cap, mf3d_u32 *len)
{
    const char *p, *e;
    char text[512];
    if (len) *len = 0;
    p = mf3d_ext_next_tag(parent, tag);
    if (!p || p >= parent_end) return MF3D_OK;
    e = mf3d_ext_find_close(p, tag);
    if (!e || e > parent_end) return MF3D_E_XML;
    mf3d_ext_text_between(p, e, text, (int)sizeof(text));
    if (text[0] == 0) return MF3D_OK;
    return mf3d_secure_content_base64_decode(text, dst, cap, len);
}

int mf3d_secure_content_parse_keystore_xml(char *xml, mf3d_secure_content *out_s)
{
    const char *ks, *p, *end, *g_end, *q, *r, *r_end;
    char tmp[512];
    mf3d_sc_group *curg;
    if (!xml || !out_s) return MF3D_E_BADARG;
    mf3d_secure_content_init(out_s);
    ks = mf3d_ext_next_tag(xml, "keystore");
    if (!ks) return MF3D_E_NOTFOUND;
    mf3d_ext_attr(ks, "UUID", tmp, (int)sizeof(tmp)); if (!tmp[0]) mf3d_ext_attr(ks, "uuid", tmp, (int)sizeof(tmp));
    scopy(out_s->uuid, (int)sizeof(out_s->uuid), tmp);
    end = mf3d_ext_find_close(ks, "keystore");
    if (!end) return MF3D_E_XML;
    p = ks;
    while ((p = mf3d_ext_next_tag(p, "consumer")) != 0 && p < end) {
        mf3d_sc_consumer *c;
        if (out_s->consumer_count >= MF3D_EXT_MAX_SC_CONSUMERS) return MF3D_E_NOSPACE;
        c = &out_s->consumers[out_s->consumer_count];
        c->consumerid[0] = c->keyid[0] = 0;
        mf3d_ext_attr(p, "consumerid", tmp, (int)sizeof(tmp)); scopy(c->consumerid, (int)sizeof(c->consumerid), tmp);
        mf3d_ext_attr(p, "keyid", tmp, (int)sizeof(tmp)); scopy(c->keyid, (int)sizeof(c->keyid), tmp);
        out_s->consumer_count++;
        p++;
    }
    p = ks;
    while ((p = mf3d_ext_next_tag(p, "resourcedatagroup")) != 0 && p < end) {
        if (out_s->group_count >= MF3D_EXT_MAX_SC_GROUPS) return MF3D_E_NOSPACE;
        curg = &out_s->groups[out_s->group_count];
        curg->keyuuid[0] = 0;
        curg->first_resource = out_s->resource_count; curg->resource_count = 0;
        curg->first_right = out_s->right_count; curg->right_count = 0;
        mf3d_ext_attr(p, "keyuuid", tmp, (int)sizeof(tmp)); scopy(curg->keyuuid, (int)sizeof(curg->keyuuid), tmp);
        g_end = mf3d_ext_find_close(p, "resourcedatagroup");
        if (!g_end) return MF3D_E_XML;
        q = p;
        while ((q = mf3d_ext_next_tag(q, "accessright")) != 0 && q < g_end) {
            mf3d_sc_accessright *ar;
            if (out_s->right_count >= MF3D_EXT_MAX_SC_RIGHTS) return MF3D_E_NOSPACE;
            ar = &out_s->rights[out_s->right_count];
            scopy(ar->keyuuid, (int)sizeof(ar->keyuuid), curg->keyuuid);
            ar->consumerindex = 0; ar->wrappingalgorithm[0] = ar->mgfalgorithm[0] = ar->digestmethod[0] = ar->cipher_b64[0] = 0;
            mf3d_ext_attr(q, "consumerindex", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &ar->consumerindex);
            r = mf3d_ext_next_tag(q, "kekparams");
            r_end = mf3d_ext_find_close(q, "accessright");
            if (!r_end) return MF3D_E_XML;
            if (r && r < r_end) {
                mf3d_ext_attr(r, "wrappingalgorithm", tmp, (int)sizeof(tmp)); scopy(ar->wrappingalgorithm, (int)sizeof(ar->wrappingalgorithm), tmp);
                mf3d_ext_attr(r, "mgfalgorithm", tmp, (int)sizeof(tmp)); scopy(ar->mgfalgorithm, (int)sizeof(ar->mgfalgorithm), tmp);
                mf3d_ext_attr(r, "digestmethod", tmp, (int)sizeof(tmp)); scopy(ar->digestmethod, (int)sizeof(ar->digestmethod), tmp);
            }
            r = mf3d_ext_next_tag(q, "CipherValue"); if (!r || r >= r_end) r = mf3d_ext_next_tag(q, "ciphervalue");
            if (r && r < r_end) { const char *ce; ce = mf3d_ext_find_close(r, "CipherValue"); if (!ce) ce = mf3d_ext_find_close(r, "ciphervalue"); if (ce && ce <= r_end) mf3d_ext_text_between(r, ce, ar->cipher_b64, (int)sizeof(ar->cipher_b64)); }
            out_s->right_count++; curg->right_count++;
            q = r_end + 1;
        }
        q = p;
        while ((q = mf3d_ext_next_tag(q, "resourcedata")) != 0 && q < g_end) {
            mf3d_sc_resource *res;
            if (out_s->resource_count >= MF3D_EXT_MAX_SC_RESOURCES) return MF3D_E_NOSPACE;
            res = &out_s->resources[out_s->resource_count];
            scopy(res->keyuuid, (int)sizeof(res->keyuuid), curg->keyuuid);
            res->path[0] = res->encryptionalgorithm[0] = res->compression[0] = 0;
            res->iv_len = res->tag_len = res->aad_len = 0;
            mf3d_ext_attr(q, "path", tmp, (int)sizeof(tmp)); scopy(res->path, (int)sizeof(res->path), tmp);
            r_end = mf3d_ext_find_close(q, "resourcedata");
            if (!r_end) return MF3D_E_XML;
            r = mf3d_ext_next_tag(q, "cekparams");
            if (r && r < r_end) {
                mf3d_ext_attr(r, "encryptionalgorithm", tmp, (int)sizeof(tmp)); scopy(res->encryptionalgorithm, (int)sizeof(res->encryptionalgorithm), tmp);
                mf3d_ext_attr(r, "compression", tmp, (int)sizeof(tmp)); scopy(res->compression, (int)sizeof(res->compression), tmp);
                if (parse_b64_child(r, r_end, "iv", res->iv, (mf3d_u32)sizeof(res->iv), &res->iv_len) != MF3D_OK) return MF3D_E_RANGE;
                if (parse_b64_child(r, r_end, "tag", res->tag, (mf3d_u32)sizeof(res->tag), &res->tag_len) != MF3D_OK) return MF3D_E_RANGE;
                if (parse_b64_child(r, r_end, "aad", res->aad, (mf3d_u32)sizeof(res->aad), &res->aad_len) != MF3D_OK) return MF3D_E_RANGE;
            }
            out_s->resource_count++; curg->resource_count++;
            q = r_end + 1;
        }
        out_s->group_count++;
        p = g_end + 1;
    }
    return MF3D_OK;
}

const mf3d_sc_resource *mf3d_secure_content_find_resource(const mf3d_secure_content *s, const char *path)
{
    mf3d_u32 i;
    if (!s || !path) return 0;
    for (i = 0; i < s->resource_count; ++i) if (streq(s->resources[i].path, path)) return &s->resources[i];
    return 0;
}

int mf3d_secure_content_decrypt_aes256_gcm(const mf3d_sc_resource *res,
                                           const mf3d_u8 cek[32],
                                           const mf3d_u8 *cipher, mf3d_u32 cipher_len,
                                           mf3d_u8 *plain, mf3d_u32 plain_cap,
                                           mf3d_u32 *plain_len)
{
    if (!res || !cek || (!cipher && cipher_len) || (!plain && cipher_len)) return MF3D_E_BADARG;
    if (res->iv_len == 0 || res->tag_len != 16UL) return MF3D_E_BADARG;
    return mf3d_zcg_decrypt_verify(cek, 32, res->iv, res->iv_len, res->aad, res->aad_len, cipher, cipher_len, res->tag, plain, plain_cap, plain_len);
}
