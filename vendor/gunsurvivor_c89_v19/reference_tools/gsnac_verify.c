/* Gun Survivor NAC catalog verifier — C89, no malloc, no floats, no stdin.
 * Usage: nac_catalog_c89_v15 ENEM31.NAC ntracks
 * Accepts the TC header variant and the three headerless variants.
 * Does not claim to know game playback timing or animation semantics.
 */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

static unsigned long rd32(FILE *fp,int *ok) {
    int a,b,c,d;
    a=fgetc(fp);b=fgetc(fp);c=fgetc(fp);d=fgetc(fp);
    if(a==EOF||b==EOF||c==EOF||d==EOF){*ok=0;return 0UL;}
    return (unsigned long)a|((unsigned long)b<<8)|((unsigned long)c<<16)|((unsigned long)d<<24);
}
static int seek_to(FILE *f,unsigned long off,unsigned long len) {
    if(off>len||off>(unsigned long)LONG_MAX)return 0;
    return fseek(f,(long)off,SEEK_SET)==0;
}
int main(int argc,char **argv) {
    FILE *in;
    unsigned long nrecords,ctrl_count,clip_count,tracks,at,k,total=0UL,
        opener,tag,record,first,second,trail,head1,head2,maxfile;
    unsigned long t,c,keys=0UL, groups=0UL;
    int ok=1,has_tc;
    char *tail;
    if(argc!=3){fputs("Usage: nac_catalog_c89_v15 file.NAC ntracks\n",stderr);return 2;}
    tracks=strtoul(argv[2],&tail,10);
    if(*tail!='\0'||tracks==0UL||tracks>512UL)return 2;
    in=fopen(argv[1],"rb");if(!in){perror("open");return 2;}
    if(fseek(in,0,SEEK_END)!=0)goto fail;
    maxfile=(unsigned long)ftell(in);
    if(!seek_to(in,0UL,maxfile))goto fail;
    nrecords=rd32(in,&ok);
    if(!ok||nrecords>1000000UL||nrecords>maxfile/112UL||!seek_to(in,4UL+nrecords*112UL,maxfile))goto fail;
    ctrl_count=rd32(in,&ok);
    if(!ok||ctrl_count<4UL||ctrl_count>(maxfile-8UL)/8UL||4UL+nrecords*112UL+4UL+ctrl_count*8UL!=maxfile)goto fail;
    first=rd32(in,&ok);second=rd32(in,&ok);
    if(!ok)goto fail;
    has_tc=((first&65535UL)==0x4354UL);
    if(has_tc) {
        clip_count=(first>>16)&255UL;
        if(((first>>24)&255UL)!=tracks||second!=nrecords-1UL||clip_count==0UL)goto fail;
        at=1UL;
    } else {
        clip_count=0UL;at=0UL;
        /* First pass: count opening sentinels to infer clip count.
           No preexisting global TC header for ENEM31/32/33. */
        for(k=0UL;k<ctrl_count;k++) {
            if(!seek_to(in,4UL+nrecords*112UL+4UL+k*8UL,maxfile))goto fail;
            head1=rd32(in,&ok);head2=rd32(in,&ok);
            if(!ok)goto fail;
            if(head2==0xffffffffUL && (head1>>16)==0x8000UL)clip_count++;
        }
        if(clip_count==0UL||clip_count%tracks!=0UL)goto fail;
        clip_count/=tracks;
    }
    for(t=0UL;t<tracks;t++) {
        for(c=0UL;c<clip_count;c++) {
            if(at>=ctrl_count||!seek_to(in,4UL+nrecords*112UL+4UL+at*8UL,maxfile))goto fail;
            opener=rd32(in,&ok);tag=rd32(in,&ok);
            if(!ok||tag!=0xffffffffUL||(opener>>16)!=0x8000UL)goto fail;
            at++;groups++;
            for(;;) {
                if(at>=ctrl_count||!seek_to(in,4UL+nrecords*112UL+4UL+at*8UL,maxfile))goto fail;
                head1=rd32(in,&ok);record=rd32(in,&ok);
                if(!ok)goto fail;
                if(record==0xffffffffUL)break;
                if(record>=nrecords)goto fail;
                keys++;total+=head1&65535UL;
                at++;
            }
            if((head1>>16)!=0x8080UL+c*0x80UL)goto fail;
            at++;
        }
    }
    trail=has_tc?tracks*10UL:tracks;
    if(at+trail!=ctrl_count)goto fail;
    for(k=at;k<ctrl_count;k++) {
        if(!seek_to(in,4UL+nrecords*112UL+4UL+k*8UL,maxfile))goto fail;
        head1=rd32(in,&ok);record=rd32(in,&ok);
        if(!ok||record>=nrecords)goto fail;
        (void)head1;
    }
    fclose(in);
    printf("PASS NAC tracks=%lu clips=%lu groups=%lu key_refs=%lu records=%lu control=%lu header=%s trailer=%lu checksum=%lu\n",
           tracks,clip_count,groups,keys,nrecords,ctrl_count,has_tc?"TC":"NO_TC",trail,total);
    return 0;
fail:
    fputs("FAIL NAC control or length violation\n",stderr);
    fclose(in);return 1;
}
