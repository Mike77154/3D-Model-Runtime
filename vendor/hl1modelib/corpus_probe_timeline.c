#include "hlfp_timeline.h"
#include "hlfp_bundle.h"
#include <stdio.h>
#include <string.h>

#define MAX_BYTES (12U*1024U*1024U)
static hlfp_u8 src[MAX_BYTES],tex[MAX_BYTES],seq[MAX_BYTES];
static hlfp_mdl mdl;
static hlfp_mdl_pose_set euler0,euler1;
static hlfp_mdl_quat_pose_set quat0,quat1;
static hlfp_error err;
static hlfp_timeline_hit hits[HLFP_MDL_MAX_EVENTS];

static int read_file(void *user,const char *path,hlfp_u8 *buf,size_t cap,size_t *out) {
    FILE *fp; long n; size_t k;
    (void)user;
    fp=fopen(path,"rb"); if(!fp) return HLFP_ERR_MISSING_DATA;
    if(fseek(fp,0,SEEK_END)!=0 || (n=ftell(fp))<0 || (unsigned long)n>cap){fclose(fp);return HLFP_ERR_CAPACITY;}
    rewind(fp);k=fread(buf,1,(size_t)n,fp);fclose(fp);
    if(k!=(size_t)n) return HLFP_ERR_IO;
    *out=k;
    return HLFP_OK;
}

int main(int argc,char **argv) {
    hlfp_bundle_buffers buf;
    hlfp_bundle_stats bs;
    int s,rc,good_e=0,bad_e=0,good_q=0,bad_q=0,good_t=0,bad_t=0,events=0,loop_test=0,loop_fail=0;
    hlfp_fixed period,f;
    int k,bones,nh;
    if(argc!=2) return 2;
    buf.main_bytes=src;buf.main_capacity=sizeof(src);
    buf.texture_bytes=tex;buf.texture_capacity=sizeof(tex);
    buf.sequence_pool=seq;buf.sequence_capacity=sizeof(seq);
    rc=hlfp_bundle_load(argv[1],read_file,0,&buf,&mdl,&bs,&err);
    if(rc!=HLFP_OK){fprintf(stderr,"READ_FAIL %s %s\n",err.message,argv[1]);return 3;}
    for(s=0;s<mdl.num_seqdesc;++s){
        const hlfp_mdl_seqdesc *sd=&mdl.seqdesc[s];
        if(sd->numframes<=0||sd->numblends<=0)continue;
        period=(hlfp_fixed)(sd->numframes-1)*HLFP_FIXED_ONE;
        if(sd->numframes>1) {
            for(k=0;k<3;k++){
                f=k==0?HLFP_FIXED_HALF:(k==1?period-HLFP_FIXED_HALF:period+HLFP_FIXED_HALF);
                rc=hlfp_mdl_decode_sequence_frame_fixed(&mdl,s,0,f,&euler0,&err);
                if(rc==HLFP_OK) good_e++;
                else {bad_e++; if(bad_e<3)fprintf(stderr,"EULER_FAIL seq %d frame %d %s in %s\n",s,f,err.message,argv[1]);}
                rc=hlfp_mdl_decode_sequence_frame_fixed_quat_ctrl(&mdl,s,f,0,&quat0,&err);
                if(rc==HLFP_OK) good_q++;
                else {bad_q++; if(bad_q<3)fprintf(stderr,"QUAT_FAIL seq %d frame %d %s in %s\n",s,f,err.message,argv[1]);}
            }
        }
        rc=hlfp_timeline_collect_frames(&mdl,s,-HLFP_FIXED_ONE,HLFP_FIXED_HALF,hits,HLFP_MDL_MAX_EVENTS,&nh,&err);
        if(rc==HLFP_OK){good_t++;events+=nh;}
        else {bad_t++;if(bad_t<3)fprintf(stderr,"TIMELINE_FAIL seq %d %s in %s\n",s,err.message,argv[1]);}
        if((sd->flags & HLFP_STUDIO_LOOPING) && sd->numframes>1){
            loop_test++;
            rc=hlfp_mdl_decode_sequence_frame_fixed(&mdl,s,0,0,&euler0,&err);
            if(rc==HLFP_OK)rc=hlfp_mdl_decode_sequence_frame_fixed(&mdl,s,0,period,&euler1,&err);
            if(rc!=HLFP_OK){loop_fail++;}
            else {
                if(euler0.num_bones!=euler1.num_bones)loop_fail++;
                else for(bones=0;bones<euler0.num_bones;++bones){
                    if(memcmp(&euler0.poses[bones],&euler1.poses[bones],sizeof(hlfp_pose))!=0){loop_fail++;break;}
                }
            }
            rc=hlfp_mdl_decode_sequence_frame_fixed_quat_ctrl(&mdl,s,0,0,&quat0,&err);
            if(rc==HLFP_OK)rc=hlfp_mdl_decode_sequence_frame_fixed_quat_ctrl(&mdl,s,period,0,&quat1,&err);
            if(rc!=HLFP_OK || quat0.num_bones!=quat1.num_bones)loop_fail++;
            else for(bones=0;bones<quat0.num_bones;++bones) {
                if(memcmp(&quat0.poses[bones],&quat1.poses[bones],sizeof(hlfp_mdl_quat_pose))!=0){loop_fail++;break;}
            }
        }
    }
    printf("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s\n",good_e,bad_e,good_q,bad_q,good_t,bad_t,events,loop_test,loop_fail,mdl.num_seqdesc,argv[1]);
    return (bad_e||bad_q||bad_t||loop_fail)?1:0;
}
