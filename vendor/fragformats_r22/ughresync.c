/* Byte-accurate differential analyzer for opaque fragMOTION UGH streams.
   C89, fixed memory, no floating point, no semantic decoding claims. */
#include <stdio.h>
#include <string.h>
#include <limits.h>
#define UGH_WINDOW 256
#define UGH_ANCHOR 16
#define UGH_EVENTS_MAX 100000UL
static unsigned char oldbuf[UGH_WINDOW+UGH_ANCHOR];
static unsigned char newbuf[UGH_WINDOW+UGH_ANCHOR];
static int file_len(FILE *f,long *len){
 if(fseek(f,0L,SEEK_END))return 0;
 *len=ftell(f);return *len>=16L;
}
static int valid_header(FILE *f,long *len){
 unsigned char h[4];
 if(!file_len(f,len)||fseek(f,0L,SEEK_SET))return 0;
 return fread(h,1,4,f)==4 && memcmp(h,"ugh0",4)==0;
}
static int read_slice(FILE *f,long offset,unsigned char *out,size_t n){
 if(fseek(f,offset,SEEK_SET))return 0;
 return fread(out,1,n,f)==n;
}
static int anchor_good(const unsigned char *p){
 int i,j,distinct=0;
 for(i=0;i<UGH_ANCHOR;i++){
  for(j=0;j<i;j++)if(p[j]==p[i])break;
  if(j==i)distinct++;
 }
 return distinct>=4;
}
static int anchor_equal(const unsigned char *a,const unsigned char *b){
 return a[0]==b[0] && memcmp(a,b,UGH_ANCHOR)==0 && anchor_good(a);
}
static void report(FILE *out,const char *action,long lo,long hi,long rn,long re){
 fprintf(out,"%s\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\n",action,lo,hi,rn,re,hi-lo,re-rn);
}
static int resync(FILE *a,FILE *b,FILE *out,long size_a,long size_b){
 long i=0,j=0,matched=0;unsigned long events=0;
 fprintf(out,"action\told_start\told_end\tnew_start\tnew_end\told_bytes\tnew_bytes\n");
 while(i<size_a && j<size_b){
  unsigned char aa,bb;long start_a,start_b,best_a=-1,best_b=-1;
  long na,nb,da,db,mincost,cost;size_t load_a,load_b;
  if(!read_slice(a,i,&aa,1)||!read_slice(b,j,&bb,1))return 0;
  if(aa==bb){i++;j++;matched++;continue;}
  if(events==UGH_EVENTS_MAX)return 0;
  start_a=i;start_b=j;
  na=size_a-i;nb=size_b-j;
  if(na>UGH_WINDOW+UGH_ANCHOR)na=UGH_WINDOW+UGH_ANCHOR;
  if(nb>UGH_WINDOW+UGH_ANCHOR)nb=UGH_WINDOW+UGH_ANCHOR;
  load_a=(size_t)na;load_b=(size_t)nb;
  if(!read_slice(a,i,oldbuf,load_a)||!read_slice(b,j,newbuf,load_b))return 0;
  mincost=2*UGH_WINDOW+2;
  /* Prioritize the closest 16-byte, nontrivial common continuation.
     An insertion or deletion is a candidate with a zero on one side. */
  for(cost=1;cost<=2*UGH_WINDOW && cost<mincost;cost++){
   for(da=0;da<=cost && da<=UGH_WINDOW;da++){
    db=cost-da;
    if(db>UGH_WINDOW)continue;
    if(da+(long)UGH_ANCHOR>na || db+(long)UGH_ANCHOR>nb)continue;
    if(anchor_equal(oldbuf+da,newbuf+db)){
     best_a=da;best_b=db;mincost=cost;break;
    }
   }
  }
  if(best_a<0){
   report(out,"opaque_replace_tail",start_a,size_a,start_b,size_b);
   i=size_a;j=size_b;events++;break;
  }
  if(best_a==0)report(out,"insert",start_a,start_a,start_b,start_b+best_b);
  else if(best_b==0)report(out,"delete",start_a,start_a+best_a,start_b,start_b);
  else if(best_a==best_b)report(out,"replace",start_a,start_a+best_a,start_b,start_b+best_b);
  else report(out,"edit_mixed",start_a,start_a+best_a,start_b,start_b+best_b);
  i+=best_a;j+=best_b;events++;
 }
 if(i<size_a){report(out,"delete_tail",i,size_a,j,j);events++;}
 else if(j<size_b){report(out,"insert_tail",i,i,j,size_b);events++;}
 fprintf(out,"summary_matched_bytes\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\n",0L,matched,0L,matched,matched,matched);
 fprintf(out,"summary_edit_events\t0\t%lu\t0\t0\t0\t0\n",events);
 fprintf(out,"summary_old_size\t0\t%ld\t0\t0\t0\t0\n",size_a);
 fprintf(out,"summary_new_size\t0\t%ld\t0\t0\t0\t0\n",size_b);
 return ferror(a)==0&&ferror(b)==0&&ferror(out)==0;
}
int main(int argc,char **argv){
 FILE *a=NULL,*b=NULL,*out=stdout;long la,lb;int ok;
 if(argc!=3&&argc!=4){fputs("Usage: ughresync old.ugh new.ugh [aligned_diff.tsv]\n",stderr);return 2;}
 a=fopen(argv[1],"rb");b=fopen(argv[2],"rb");
 if(a==NULL||b==NULL){fputs("Cannot open UGH input\n",stderr);if(a)fclose(a);if(b)fclose(b);return 2;}
 if(!valid_header(a,&la)||!valid_header(b,&lb)){
  fputs("Invalid UGH signature/size\n",stderr);fclose(a);fclose(b);return 2;
 }
 if(argc==4){out=fopen(argv[3],"wb");if(!out){fclose(a);fclose(b);return 2;}}
 ok=resync(a,b,out,la,lb);
 if(out!=stdout)fclose(out);
 fclose(a);fclose(b);
 if(!ok)fputs("Cannot compare streams\n",stderr);
 return ok?0:1;
}
