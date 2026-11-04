#include "../include/modelrt_prj89.h"
#include "prj_c89.h"
#include <string.h>
size_t modelrt_prj89_workspace_bytes(void)
{return sizeof(PRJ_Context)+sizeof(PRJ_CRSInfo)+sizeof(void*);}
int modelrt_prj89_summary(const char *wkt,void *workspace,size_t n,char *out,size_t cap)
{
    PRJ_Context *ctx; PRJ_CRSInfo *info;
    if(!wkt||!workspace||!out||!cap)return PRJ_ERR_NULL;
    if(n<modelrt_prj89_workspace_bytes())return PRJ_ERR_OUTPUT_SMALL;
    /* Caller must provide memory aligned for PRJ_Context. */
    ctx=(PRJ_Context*)workspace;info=(PRJ_CRSInfo*)((char*)workspace+sizeof(PRJ_Context));
    prj_context_init(ctx);prj_crs_info_init(info);
    if(prj_parse_prj(ctx,info,wkt)!=PRJ_OK)return PRJ_ERR_SYNTAX;
    return prj_write_summary(info,out,cap);
}
