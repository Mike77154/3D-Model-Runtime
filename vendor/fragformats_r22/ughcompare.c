#include "vendor_fragmotion/ugh_forensics.h"
#include <stdio.h>
int main(int argc,char **argv){
 FILE *a,*b,*out=stdout;int ok;
 if(argc!=3&&argc!=4){fputs("Usage: ughcompare before.ugh after.ugh [diff.tsv]\n",stderr);return 2;}
 a=fopen(argv[1],"rb");b=fopen(argv[2],"rb");
 if(!a||!b){fputs("Cannot open UGH inputs\n",stderr);if(a)fclose(a);if(b)fclose(b);return 2;}
 if(argc==4){out=fopen(argv[3],"wb");if(!out){fclose(a);fclose(b);return 2;}}
 ok=ugh_compare(a,b,out);
 if(out!=stdout)fclose(out);
 fclose(a);fclose(b);
 if(!ok)fputs("Invalid UGH input or read error\n",stderr);
 return ok?0:1;
}
