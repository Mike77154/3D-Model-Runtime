#include "mf3d.h"
#include <string.h>

extern int mf3d_parse_fix(const char *s, mf3d_fix *out);
extern int mf3d_parse_u32(const char *s, mf3d_u32 *out);

static int tagname_eq(const char *p, const char *name)
{
    int i = 0;
    if (*p != '<') return 0;
    p++;
    if (*p == '/') return 0;
    while (name[i]) { if (p[i] != name[i]) return 0; i++; }
    return (p[i] == ' ' || p[i] == '\t' || p[i] == '\r' || p[i] == '\n' || p[i] == '>' || p[i] == '/');
}
static void copy_attr(const char *tag, const char *attr, char *out, int outcap)
{
    int alen, i; const char *p; char q;
    if (outcap <= 0) return;
    out[0] = 0;
    alen = 0;
    while (attr[alen]) alen++;
    p = tag;
    while (*p && *p != '>') {
        if ((p == tag || p[-1] == ' ' || p[-1] == '\t' || p[-1] == '\n' || p[-1] == '\r') ) {
            for (i = 0; i < alen && p[i] == attr[i]; ++i) ;
            if (i == alen && p[i] == '=') {
                q = p[i + 1]; if (q != '\'' && q != '"') return;
                p += i + 2; i = 0;
                while (*p && *p != q && i < outcap - 1) out[i++] = *p++;
                out[i] = 0; return;
            }
        }
        p++;
    }
}
static const char *next_tag(const char *p, const char *name)
{
    while (*p) { if (*p == '<' && tagname_eq(p, name)) return p; p++; }
    return 0;
}
static void set_identity(mf3d_fix *t)
{
    int i; for (i = 0; i < 12; ++i) t[i] = 0;
    t[0] = t[5] = t[10] = (mf3d_fix)(1L << 16);
}
static void parse_transform(const char *s, mf3d_fix *t)
{
    int i; const char *p; char num[64]; int n;
    set_identity(t); if (!s || !s[0]) return; p = s;
    for (i = 0; i < 12; ++i) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        n = 0;
        while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && n < 63) num[n++] = *p++;
        num[n] = 0;
        if (n > 0) mf3d_parse_fix(num, &t[i]);
    }
}

/* 3MF property resource references are not vertex indices. The property
 * group's ID selects a resource and p1/p2/p3 index that resource's entries.
 * Previous versions accepted arbitrarily large p-indices and produced
 * plausible-looking geometry with invalid materials. Reject instead.
 * No heap, C89, bounded 128-group table. */
#define MF3D_PROPERTY_GROUP_CAP 128
static int property_name(const char *tag, const char *name)
{
    const char *s, *e, *local;
    unsigned int n;
    if (*tag != '<' || tag[1] == '/') return 0;
    s=tag+1;e=s;
    while (*e && *e!=' ' && *e!='\t' && *e!='\r' && *e!='\n' && *e!='/' && *e!='>') e++;
    local=s;
    while(s<e){if(*s==':')local=s+1;s++;}
    n=(unsigned int)strlen(name);
    return (unsigned int)(e-local)==n && !strncmp(local,name,n);
}
static int property_close(const char *tag,const char *name)
{
    const char *s, *e, *local;
    unsigned int n;
    if (tag[0]!='<' || tag[1]!='/') return 0;
    s=tag+2;e=s;
    while(*e && *e!='>' && *e!=' ' && *e!='\n')e++;
    local=s;while(s<e){if(*s==':')local=s+1;s++;}
    n=(unsigned int)strlen(name);
    return (unsigned int)(e-local)==n&&!strncmp(local,name,n);
}
static int property_group_kind(const char *s)
{
    if(property_name(s,"colorgroup"))return 1;
    if(property_name(s,"basematerials"))return 2;
    if(property_name(s,"tex2dgroup"))return 3;
    if(property_name(s,"compositematerials"))return 4;
    if(property_name(s,"multiproperties"))return 5;
    return 0;
}
static const char *property_group_name(int k)
{
    if(k==1)return "colorgroup";
    if(k==2)return "basematerials";
    if(k==3)return "tex2dgroup";
    if(k==4)return "compositematerials";
    return "multiproperties";
}
static int property_child(int k,const char *s)
{
    if(k==1)return property_name(s,"color");
    if(k==2)return property_name(s,"base");
    if(k==3)return property_name(s,"tex2coord");
    if(k==4)return property_name(s,"composite");
    return property_name(s,"multi");
}
static int validate_property_indices(const char *xml,const mf3d_model *m)
{
    mf3d_u32 ids[MF3D_PROPERTY_GROUP_CAP],counts[MF3D_PROPERTY_GROUP_CAP];
    int n=0,k,j,found;const char *p,*q,*end;char tmp[128];mf3d_u32 id;
    p=xml;
    while(*p){
        if(*p!='<'){p++;continue;}
        k=property_group_kind(p);
        if(!k){p++;continue;}
        if(n==MF3D_PROPERTY_GROUP_CAP)return MF3D_E_NOSPACE;
        copy_attr(p,"id",tmp,(int)sizeof(tmp));
        if(!tmp[0] || mf3d_parse_u32(tmp,&id)!=MF3D_OK)return MF3D_E_XML;
        for(j=0;j<n;j++)if(ids[j]==id)return MF3D_E_XML;
        ids[n]=id;counts[n]=0;
        q=p;end=0;
        while((q=strchr(q+1,'<'))!=0){
            if(property_close(q,property_group_name(k))){end=q;break;}
            if(property_child(k,q))counts[n]++;
        }
        if(!end)return MF3D_E_XML;
        n++;p=end+1;
    }
    for(id=0;id<m->triangle_count;id++){
        const mf3d_tri*t=&m->triangles[id];
        if(!t->pid){if(t->p1||t->p2||t->p3)return MF3D_E_RANGE;continue;}
        found=-1;
        for(j=0;j<n;j++)if(ids[j]==t->pid){found=j;break;}
        if(found<0)return MF3D_E_UNSUPPORTED;
        if(t->p1>=counts[found] || t->p2>=counts[found] || t->p3>=counts[found])return MF3D_E_RANGE;
    }
    return MF3D_OK;
}

int mf3d_parse_model_xml(char *xml, mf3d_model *m)
{
    const char *p, *end;
    char tmp[128];
    mf3d_object *cur;
    if (!xml || !m) return MF3D_E_BADARG;
    p = next_tag(xml, "model");
    if (p) {
        copy_attr(p, "unit", m->unit, (int)sizeof(m->unit));
        copy_attr(p, "xml:lang", m->lang, (int)sizeof(m->lang));
    }
    p = xml;
    while ((p = next_tag(p, "object")) != 0) {
        if (m->object_count >= MF3D_MAX_OBJECTS) return MF3D_E_NOSPACE;
        cur = &m->objects[m->object_count];
        cur->vertex_first = m->vertex_count;
        cur->tri_first = m->triangle_count;
        cur->vertex_count = 0; cur->tri_count = 0; cur->id = 0; cur->type[0] = 0; cur->name[0] = 0;
        copy_attr(p, "id", tmp, (int)sizeof(tmp)); mf3d_parse_u32(tmp, &cur->id);
        copy_attr(p, "type", cur->type, (int)sizeof(cur->type));
        copy_attr(p, "name", cur->name, (int)sizeof(cur->name));
        end = p;
        while (*end) {
            if (end[0] == '<' && end[1] == '/' && end[2] == 'o' && end[3] == 'b' && end[4] == 'j') break;
            end++;
        }
        {
            const char *q = p;
            while ((q = next_tag(q, "vertex")) != 0 && q < end) {
                mf3d_vec3 *v;
                if (m->vertex_count >= MF3D_MAX_VERTICES) return MF3D_E_NOSPACE;
                v = &m->vertices[m->vertex_count];
                copy_attr(q, "x", tmp, (int)sizeof(tmp)); mf3d_parse_fix(tmp, &v->x);
                copy_attr(q, "y", tmp, (int)sizeof(tmp)); mf3d_parse_fix(tmp, &v->y);
                copy_attr(q, "z", tmp, (int)sizeof(tmp)); mf3d_parse_fix(tmp, &v->z);
                m->vertex_count++; cur->vertex_count++;
                q++;
            }
            q = p;
            while ((q = next_tag(q, "triangle")) != 0 && q < end) {
                mf3d_tri *t;
                if (m->triangle_count >= MF3D_MAX_TRIANGLES) return MF3D_E_NOSPACE;
                t = &m->triangles[m->triangle_count];
                t->pid = t->p1 = t->p2 = t->p3 = 0;
                copy_attr(q, "v1", tmp, (int)sizeof(tmp)); mf3d_parse_u32(tmp, &t->v1); t->v1 += cur->vertex_first;
                copy_attr(q, "v2", tmp, (int)sizeof(tmp)); mf3d_parse_u32(tmp, &t->v2); t->v2 += cur->vertex_first;
                copy_attr(q, "v3", tmp, (int)sizeof(tmp)); mf3d_parse_u32(tmp, &t->v3); t->v3 += cur->vertex_first;
                copy_attr(q, "pid", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &t->pid);
                copy_attr(q, "p1", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &t->p1);
                copy_attr(q, "p2", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &t->p2);
                copy_attr(q, "p3", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &t->p3);
                m->triangle_count++; cur->tri_count++;
                q++;
            }
        }
        m->object_count++;
        p = end;
    }
    p = xml;
    while ((p = next_tag(p, "item")) != 0) {
        mf3d_builditem *bi;
        if (m->builditem_count >= MF3D_MAX_BUILDITEMS) return MF3D_E_NOSPACE;
        bi = &m->builditems[m->builditem_count]; bi->objectid = 0; set_identity(bi->transform);
        copy_attr(p, "objectid", tmp, (int)sizeof(tmp)); mf3d_parse_u32(tmp, &bi->objectid);
        copy_attr(p, "transform", tmp, (int)sizeof(tmp)); parse_transform(tmp, bi->transform);
        m->builditem_count++;
        p++;
    }
    return validate_property_indices(xml,m);
}
