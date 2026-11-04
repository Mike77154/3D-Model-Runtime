/* FragFormats R22: linked object stream UGH *layout* reader.
 * Evidence: fragMOTION 1.3.2 native importer 0x00469FA1 and recursive
 * 0x0046A220: objectRecord.firstChild/nextSibling are absolute IStream
 * seek offsets, not indices into a presumed contiguous array.
 * These are binary-structure probes, NOT a complete UGH renderer.
 * Strict ISO C89, bounded fixed storage, no heap/no float. */
#include <stdio.h>
#include <limits.h>
#include <string.h>
#define UGH_MAX 4096U
#define UGH_NULL 0xffffffffUL
#define UGH_MAX_PROPERTIES 16384UL

typedef struct Obj {
 unsigned long at,f[7];
 long parent;
 unsigned long type,id,flags;
 int data_ok;
} Obj;
static Obj objs[UGH_MAX];
static unsigned char bytes[64];
static unsigned long count, faults, revisits, properties;
static unsigned long rd32(const unsigned char *p){
 return (unsigned long)p[0]|((unsigned long)p[1]<<8)|
        ((unsigned long)p[2]<<16)|((unsigned long)p[3]<<24);
}
static int get(FILE *f,unsigned long at,unsigned char *dst,size_t n,unsigned long sz){
 if(at>sz||at>(unsigned long)LONG_MAX||n>(size_t)(sz-at))return 0;
 if(fseek(f,(long)at,SEEK_SET))return 0;
 return fread(dst,1,n,f)==n;
}
static long find(unsigned long addr){
 unsigned long i;
 for(i=0;i<count;i++)if(objs[i].at==addr)return (long)i;
 return -1;
}
static int parse(FILE *f,FILE *out,FILE *dot){
 long fl;unsigned long sz,stream,root,version,nexpected,i,j;
 if(fseek(f,0,SEEK_END))return 0;
 fl=ftell(f);if(fl<16L)return 0;sz=(unsigned long)fl;
 if(!get(f,0UL,bytes,16,sz)||memcmp(bytes,"ugh0",4))return 0;
 stream=rd32(bytes+4);
 fprintf(out,"kind\tobject\tparent\toffset\tfield\tvalue\n");
 fprintf(out,"outer\t-\t-\t4\tstream_header_offset\t%lu\n",stream);
 if(stream<16UL || !get(f,stream,bytes,16,sz)){
  fprintf(out,"status\t-\t-\t0\tresult\tUNREADABLE_NESTED_STREAM_HEADER\n");return 1;
 }
 version=rd32(bytes+4);root=rd32(bytes+8);nexpected=rd32(bytes+12);
 fprintf(out,"inner\t-\t-\t%lu\tversion\t%lu\n",stream+4,version);
 fprintf(out,"inner\t-\t-\t%lu\tdataStart\t%lu\n",stream+8,root);
 fprintf(out,"inner\t-\t-\t%lu\tnumObjects\t%lu\n",stream+12,nexpected);
 fprintf(out,"disclaimer\t-\t-\t0\tstatus\tNATIVE_LAYOUT_FROM_DISASSEMBLY_BUT_NO_GENUINE_UGH_VALIDATION\n");
 if(version<0x10000UL||version>0x10001UL){
  fprintf(out,"status\t-\t-\t0\tresult\tOUT_OF_SUPPORTED_VERSION_RANGE\n");return 1;
 }
 if(root<16UL || root>sz || 28UL>sz-root){
  fprintf(out,"status\t-\t-\t0\tresult\tROOT_RECORD_OUT_OF_BOUNDS\n");return 1;
 }
 memset(objs,0,sizeof(objs));
 count=1;faults=0;revisits=0;properties=0;
 objs[0].at=root;objs[0].parent=-1;
 if(dot)fprintf(dot,"// Format inferred from actual fragMOTION importer; no native UGH sample tested\ndigraph UGH_objects {\n");
 for(i=0;i<count;i++){
  Obj *o=&objs[i];
  const char *fields[7]={"offset","numAttr","attributes","extraData","firstChild","nextSibling","reserved"};
  if(!get(f,o->at,bytes,28,sz)){
   fprintf(out,"problem\t%lu\t%ld\t%lu\tobject\tTRUNCATED\n",i,o->parent,o->at);faults++;continue;
  }
  for(j=0;j<7;j++)o->f[j]=rd32(bytes+j*4UL);
  fprintf(out,"object\t%lu\t%ld\t%lu\trecord\tFOUND\n",i,o->parent,o->at);
  for(j=0;j<7;j++)fprintf(out,"field\t%lu\t%ld\t%lu\t%s\t%lu\n",i,o->parent,o->at+j*4UL,fields[j],o->f[j]);
  if(o->f[0]>=16UL && get(f,o->f[0],bytes,12,sz)){
   o->type=rd32(bytes);o->id=rd32(bytes+4);o->flags=rd32(bytes+8);o->data_ok=1;
   fprintf(out,"object_data\t%lu\t%ld\t%lu\ttype\t%lu\n",i,o->parent,o->f[0],o->type);
   fprintf(out,"object_data\t%lu\t%ld\t%lu\tpacked_id\t%lu\n",i,o->parent,o->f[0]+4UL,o->id);
   fprintf(out,"object_data\t%lu\t%ld\t%lu\tflags\t%lu\n",i,o->parent,o->f[0]+8UL,o->flags);
   {
    const char *category=o->type==0x2eUL?"IMesh":
      o->type==0x38UL?"IMatrix4x4":
      o->type==0x3eUL?"IMeshKeyList":"UNCLASSIFIED";
    fprintf(out,"object_data\t%lu\t%ld\t%lu\tclass_candidate\t%s\n",i,o->parent,o->f[0],category);
   }
   /* Type-specific opaque payloads: only fixed, proven read lengths.
      These are raw words, NOT evaluated skeleton/animation frames. */
   if(o->f[3]>=16UL && o->f[3]<sz){
    if(o->type==0x2eUL && get(f,o->f[3],bytes,8,sz)){
     fprintf(out,"mesh_payload\t%lu\t%ld\t%lu\tcount_word0\t%lu\n",i,o->parent,o->f[3],rd32(bytes));
     fprintf(out,"mesh_payload\t%lu\t%ld\t%lu\tcount_word1\t%lu\n",i,o->parent,o->f[3]+4UL,rd32(bytes+4));
    }else if(o->type==0x38UL && get(f,o->f[3],bytes,64,sz)){
     unsigned long k;
     for(k=0;k<16UL;k++)
      fprintf(out,"matrix_raw\t%lu\t%ld\t%lu\tword_%lu\t%lu\n",i,o->parent,o->f[3]+k*4UL,k,rd32(bytes+k*4UL));
    }else if(o->type==0x3eUL && get(f,o->f[3],bytes,12,sz)){
     fprintf(out,"meshkeys_raw\t%lu\t%ld\t%lu\tnumFrames_candidate\t%lu\n",i,o->parent,o->f[3],rd32(bytes));
     fprintf(out,"meshkeys_raw\t%lu\t%ld\t%lu\tsecond_word\t%lu\n",i,o->parent,o->f[3]+4UL,rd32(bytes+4));
     fprintf(out,"meshkeys_raw\t%lu\t%ld\t%lu\tthird_word\t%lu\n",i,o->parent,o->f[3]+8UL,rd32(bytes+8));
    }
   }
   if(dot)fprintf(dot,"  obj%lu [label=\"%lu type=0x%lX\"];\n",i,i,o->type);
  }else{
   fprintf(out,"problem\t%lu\t%ld\t%lu\tobject_data\tINVALID_OR_TRUNCATED\n",i,o->parent,o->f[0]);faults++;
   if(dot)fprintf(dot,"  obj%lu [label=\"%lu unreadable\"];\n",i,i);
  }
  if(o->f[1]){
   if(o->f[1]>UGH_MAX_PROPERTIES || o->f[2]<16UL || o->f[2]>sz || o->f[1]>(sz-o->f[2])/8UL){
    fprintf(out,"problem\t%lu\t%ld\t%lu\tattributes\tOUT_OF_BOUNDS\n",i,o->parent,o->f[2]);faults++;
   }else{
    unsigned long k;
    for(k=0;k<o->f[1]&&properties<UGH_MAX_PROPERTIES;k++){
     unsigned long at=o->f[2]+k*8UL;
     if(!get(f,at,bytes,8,sz)){faults++;break;}
     fprintf(out,"property\t%lu\t%ld\t%lu\tpropID\t%lu\n",i,o->parent,at,rd32(bytes));
     fprintf(out,"property\t%lu\t%ld\t%lu\tvalue\t%lu\n",i,o->parent,at+4UL,rd32(bytes+4));
     properties++;
    }
   }
  }
  /* firstChild and nextSibling in native code are file positions. */
  for(j=4;j<=5;j++){
   unsigned long to=o->f[j];
   if(to==UGH_NULL)continue;
   if(to<16UL||to>sz||28UL>sz-to){
    fprintf(out,"problem\t%lu\t%ld\t%lu\t%s\tOUT_OF_BOUNDS\n",i,o->parent,to,fields[j]);faults++;continue;
   }
   {
    long previous=find(to);
    if(previous>=0){
     fprintf(out,"problem\t%lu\t%ld\t%lu\t%s\tREVISIT_NODE_%ld\n",i,o->parent,to,fields[j],previous);
     revisits++;
     if(dot)fprintf(dot,"  obj%lu -> obj%ld [label=\"%s revisit\",color=red];\n",i,previous,fields[j]);
    }else if(count>=UGH_MAX){
     fprintf(out,"problem\t%lu\t%ld\t%lu\t%s\tOBJECT_LIMIT\n",i,o->parent,to,fields[j]);faults++;
    }else{
     long owner=(j==4)?(long)i:o->parent;
     unsigned long slot=count++;
     objs[slot].at=to;objs[slot].parent=owner;
     fprintf(out,"edge\t%lu\t%ld\t%lu\t%s\t%lu\n",i,o->parent,to,fields[j],slot);
     if(dot)fprintf(dot,"  obj%lu -> obj%lu [label=\"%s\"];\n",i,slot,fields[j]);
    }
   }
  }
 }
 fprintf(out,"summary\t-\t-\t0\tobjects_walked\t%lu\n",count);
 fprintf(out,"summary\t-\t-\t0\tproperties_examined\t%lu\n",properties);
 fprintf(out,"summary\t-\t-\t0\tfaults\t%lu\n",faults);
 fprintf(out,"summary\t-\t-\t0\trevisits\t%lu\n",revisits);
 fprintf(out,"summary\t-\t-\t0\tinteroperability\tNOT_VALIDATED_WITH_REAL_UGH\n");
 if(dot)fprintf(dot,"}\n");
 return !ferror(out)&&(!dot||!ferror(dot));
}
int main(int argc,char **argv){FILE *in,*out=stdout,*dot=NULL;int ok;
 if(argc<2||argc>4){fputs("Usage: ughtree model.ugh [tree.tsv [tree.dot]]\n",stderr);return 2;}
 in=fopen(argv[1],"rb");if(!in){perror(argv[1]);return 2;}
 if(argc>=3){out=fopen(argv[2],"wb");if(!out){perror(argv[2]);fclose(in);return 2;}}
 if(argc==4){dot=fopen(argv[3],"wb");if(!dot){perror(argv[3]);if(out!=stdout)fclose(out);fclose(in);return 2;}}
 ok=parse(in,out,dot);
 if(dot)fclose(dot);
 if(out!=stdout)fclose(out);
 fclose(in);
 if(!ok)fputs("Cannot parse UGH stream\n",stderr);
 return ok?0:1;
}
