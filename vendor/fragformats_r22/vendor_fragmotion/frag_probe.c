#include "frag_probe.h"
#include <string.h>
#include <stdio.h>

static unsigned long u32le(const unsigned char *b) {
 return (unsigned long)b[0]|((unsigned long)b[1]<<8)|((unsigned long)b[2]<<16)|((unsigned long)b[3]<<24);
}
int frag_probe(FILE *fp, const char *ext) {
 unsigned char b[4096]; size_t n; size_t i; int found=0;
 if (fseek(fp,0L,SEEK_SET)!=0) return 0;
 n=fread(b,1,sizeof(b),fp);
 printf("Fragmotion probe: extension=%s bytes_read=%lu\n",ext,(unsigned long)n);
 if(n>=9 && memcmp(b+4,"arg00",5)==0) {
  printf("  ARG: header flags/version_word=%lu, magic=arg00 at offset 4 [validated on included examples]\n",u32le(b));
  if(n>9) {char name[33]; size_t k;for(k=0;k<32;k++) {unsigned char c=(k+9<n)?b[k+9]:0;name[k]=(c>=32 && c<127)?(char)c:0;if(!name[k])break;}name[k]=0;printf("  ARG: first name='%s'\n",name);}
  return 1;
 }
 for(i=0;i+4<=n;i++) {
  if(memcmp(b+i,"ugh0",4)==0) { printf("  UGH marker found at byte %lu (format structure not yet validated)\n",(unsigned long)i);found=1;break;}
 }
 if(strcmp(ext,"ugh")==0) {
  if(n>=16 && memcmp(b,"ugh0",4)==0) {
   printf("  UGH: 16-byte header starts with ugh0; other 12 bytes are opaque, NOT decoded.\n");
   printf("  UGH: header words at offsets 4/8/12 (raw little-endian) = %08lX %08lX %08lX\n",u32le(b+4),u32le(b+8),u32le(b+12));
   return 1;
  }
  printf("  UGH: no validated 16-byte magic/header.\n");return 0;
 }
 return found;
}
