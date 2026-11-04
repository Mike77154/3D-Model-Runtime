/* FragFormats R22. SDK-structure candidate graph inspector.
 * The placement of objectRecord at streamHeader.dataStart is UNVERIFIED
 * against native UGH. Outputs hypotheses, not decoded native objects.
 * Strict ISO C89; bounded static storage; no floating point or heap. */
#include <stdio.h>
#include <string.h>
#include <limits.h>
#define UGH_MAX_OBJECTS 4096U
#define UGH_REC_SIZE 28UL
#define UGH_PROP_SIZE 8UL
#define UGH_NONE 0xffffffffUL
#define UGH_MAX_PROPS_REPORT 16384UL

typedef struct UGHRecord {unsigned long f[7];} UGHRecord;
static UGHRecord recs[UGH_MAX_OBJECTS];
static unsigned char colors[UGH_MAX_OBJECTS];
static unsigned char reached[UGH_MAX_OBJECTS];
static unsigned short stack_nodes[UGH_MAX_OBJECTS];
static unsigned char stack_state[UGH_MAX_OBJECTS];
static unsigned short queue[UGH_MAX_OBJECTS];
static unsigned char raw[28];
static unsigned long nobj;

static unsigned long u32(const unsigned char *p){
 return (unsigned long)p[0]|((unsigned long)p[1]<<8)|
        ((unsigned long)p[2]<<16)|((unsigned long)p[3]<<24);
}
static int fetch(FILE *f,unsigned long off,unsigned char *dst,size_t size,unsigned long filesize){
 if(off>filesize || size>(size_t)(filesize-off) || off>(unsigned long)LONG_MAX)return 0;
 if(fseek(f,(long)off,SEEK_SET))return 0;
 return fread(dst,1,size,f)==size;
}
/* Both index conventions are *hypotheses*. -1=missing; -2=invalid. */
static long decode(unsigned long x,int onebased){
 if(x==UGH_NONE || (onebased && x==0))return -1;
 if(onebased){if(x<1UL||x>nobj)return -2;return (long)(x-1UL);}
 if(x>=nobj)return -2;
 return (long)x;
}
typedef struct Stats {unsigned long edges,invalid,cycles,reachable,roots, selfref;} Stats;
static void stats_graph(int onebased,Stats *st){
 unsigned long i,top,front,back;
 memset(st,0,sizeof(*st));
 memset(colors,0,sizeof(colors));
 memset(reached,0,sizeof(reached));
 for(i=0;i<nobj;i++){
  long a=decode(recs[i].f[4],onebased),b=decode(recs[i].f[5],onebased);
  if(a==-2)st->invalid++;else if(a>=0){st->edges++;if((unsigned long)a==i)st->selfref++;}
  if(b==-2)st->invalid++;else if(b>=0){st->edges++;if((unsigned long)b==i)st->selfref++;}
 }
 /* Iterative DFS across both pointer types: detects cycles without recursion. */
 for(i=0;i<nobj;i++){
  if(colors[i])continue;
  top=1;stack_nodes[0]=(unsigned short)i;stack_state[0]=0;
  colors[i]=1;
  while(top){
   unsigned long node=stack_nodes[top-1];
   unsigned char state=stack_state[top-1];
   long next;
   if(state==2){colors[node]=2;top--;continue;}
   stack_state[top-1]=(unsigned char)(state+1U);
   next=decode(recs[node].f[state==0?4:5],onebased);
   if(next<0)continue;
   if(colors[next]==1){st->cycles++;continue;}
   if(colors[next]==0){
    if(top>=UGH_MAX_OBJECTS){st->invalid++;continue;}
    stack_nodes[top]=(unsigned short)next;stack_state[top]=0;
    colors[next]=1;top++;
   }
  }
 }
 if(nobj){
  front=0;back=1;queue[0]=0;reached[0]=1;
  while(front<back){
   unsigned long v=queue[front++];
   int edge;
   for(edge=4;edge<=5;edge++){
    long to=decode(recs[v].f[edge],onebased);
    if(to>=0&&!reached[to]){reached[to]=1;queue[back++]=(unsigned short)to;}
   }
  }
  st->reachable=back;
 }
 st->roots=nobj-st->reachable;
}
static void print_stats(FILE *o,const char *which,const Stats *s){
 fprintf(o,"indexing_candidate\t%s\tedges\t%lu\n",which,s->edges);
 fprintf(o,"indexing_candidate\t%s\tinvalid_refs\t%lu\n",which,s->invalid);
 fprintf(o,"indexing_candidate\t%s\tcycles\t%lu\n",which,s->cycles);
 fprintf(o,"indexing_candidate\t%s\tself_refs\t%lu\n",which,s->selfref);
 fprintf(o,"indexing_candidate\t%s\treachable_from_record0\t%lu\n",which,s->reachable);
}
static int process(FILE *f,FILE *out,FILE *dot){
 long fl;unsigned long total,dataStart,vers,i,j,invalidAttr=0,propsEmitted=0;
 Stats a,b;int choice;const char *basis;
 if(fseek(f,0,SEEK_END))return 0;
 fl=ftell(f);if(fl<16L)return 0;
 total=(unsigned long)fl;
 if(!fetch(f,0UL,raw,16,total)||memcmp(raw,"ugh0",4))return 0;
 vers=u32(raw+4);dataStart=u32(raw+8);nobj=u32(raw+12);
 fprintf(out,"kind\tobject\tfield\tvalue\n");
 fprintf(out,"header\t-\tversion\t%lu\n",vers);
 fprintf(out,"header\t-\tdataStart\t%lu\n",dataStart);
 fprintf(out,"header\t-\tnumObjects\t%lu\n",nobj);
 fprintf(out,"header\t-\tfileSize\t%lu\n",total);
 fprintf(out,"disclaimer\t-\tstatus\tUNVERIFIED_CONTIGUOUS_OBJECT_TABLE_HYPOTHESIS\n");
 if(nobj==0UL){fprintf(out,"table\t-\tstatus\tNO_OBJECTS_DECLARED\n");return 1;}
 if(nobj>UGH_MAX_OBJECTS || dataStart<16UL || dataStart>total ||
    nobj>(total-dataStart)/UGH_REC_SIZE){
  fprintf(out,"table\t-\tstatus\tNO_BOUNDED_TABLE_AT_DATASTART\n");return 1;
 }
 for(i=0;i<nobj;i++){
  if(!fetch(f,dataStart+i*UGH_REC_SIZE,raw,28,total))return 0;
  for(j=0;j<7;j++)recs[i].f[j]=u32(raw+j*4);
 }
 fprintf(out,"table\t-\tstatus\tBOUNDED_CANDIDATE_ONLY\n");
 stats_graph(0,&a);stats_graph(1,&b);
 print_stats(out,"zero_based_FFFFFFFF_null",&a);
 print_stats(out,"one_based_zero_null",&b);
 /* No tie-break guesswork: if scores tie, report ambiguity and skip DOT. */
 if(a.invalid<b.invalid)choice=0;
 else if(b.invalid<a.invalid)choice=1;
 else if(a.cycles<b.cycles)choice=0;
 else if(b.cycles<a.cycles)choice=1;
 else if(a.reachable>b.reachable)choice=0;
 else if(b.reachable>a.reachable)choice=1;
 else choice=-1;
 basis=choice==0?"zero_based":choice==1?"one_based":"AMBIGUOUS";
 fprintf(out,"table\t-\tpreferred_indexing_hypothesis\t%s\n",basis);
 for(i=0;i<nobj;i++){
  const char *names[7]={"offset","numAttr","attributes","extraData","firstChild","nextSibling","reserved"};
  for(j=0;j<7;j++)fprintf(out,"object\t%lu\t%s\t%lu\n",i,names[j],recs[i].f[j]);
  /* A property array at 'attributes' is only a hypothesis. Do not dereference if invalid. */
  if(recs[i].f[1] && recs[i].f[1] <= UGH_MAX_PROPS_REPORT &&
     recs[i].f[2]>=16UL && recs[i].f[2]<=total &&
     recs[i].f[1]<=(total-recs[i].f[2])/UGH_PROP_SIZE){
    unsigned long k;
    for(k=0;k<recs[i].f[1] && propsEmitted<UGH_MAX_PROPS_REPORT;k++){
     if(!fetch(f,recs[i].f[2]+k*UGH_PROP_SIZE,raw,8,total))return 0;
     fprintf(out,"property_candidate\t%lu\tpropID_%lu\t%lu\n",i,k,u32(raw));
     fprintf(out,"property_candidate\t%lu\tvalue_%lu\t%lu\n",i,k,u32(raw+4));
     propsEmitted++;
    }
  }else if(recs[i].f[1])invalidAttr++;
 }
 fprintf(out,"property_probe\t-\tinvalid_or_unverified_arrays\t%lu\n",invalidAttr);
 fprintf(out,"property_probe\t-\tproperty_records_examined\t%lu\n",propsEmitted);
 if(dot){
  fprintf(dot,"// HYPOTHETICAL object graph, NOT verified UGH serialization.\n");
  fprintf(dot,"digraph UGH_candidate {\n  node [shape=box];\n");
  for(i=0;i<nobj;i++)fprintf(dot,"  obj%lu [label=\"object %lu\"];\n",i,i);
  if(choice>=0){
   for(i=0;i<nobj;i++){
    long next=decode(recs[i].f[4],choice);
    if(next>=0)fprintf(dot,"  obj%lu -> obj%ld [label=\"firstChild\"];\n",i,next);
    next=decode(recs[i].f[5],choice);
    if(next>=0)fprintf(dot,"  obj%lu -> obj%ld [style=dashed,label=\"nextSibling\"];\n",i,next);
   }
  } else fprintf(dot,"  // ambiguous index base; edges intentionally omitted\n");
  fprintf(dot,"}\n");
 }
 return !ferror(out) && (!dot || !ferror(dot));
}
int main(int argc,char **argv){FILE *in,*out=stdout,*dot=NULL;int ok;
 if(argc<2||argc>4){fputs("Usage: ughgraph candidate.ugh [candidate.tsv [candidate.dot]]\n",stderr);return 2;}
 in=fopen(argv[1],"rb");if(!in){perror(argv[1]);return 2;}
 if(argc>=3){out=fopen(argv[2],"wb");if(!out){perror(argv[2]);fclose(in);return 2;}}
 if(argc==4){dot=fopen(argv[3],"wb");if(!dot){perror(argv[3]);if(out!=stdout)fclose(out);fclose(in);return 2;}}
 ok=process(in,out,dot);
 if(dot)fclose(dot);
 if(out!=stdout)fclose(out);
 fclose(in);
 if(!ok)fputs("UGH candidate probe failed (invalid file or I/O)\n",stderr);
 return ok?0:1;
}
