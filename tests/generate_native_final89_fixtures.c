/* Test-only fixture generator, ISO C89. Produces tiny files in working directory.
 * Do not include the generated binary/text models in source-only distributions. */
#include <stdio.h>
#include <string.h>
static int put(const char *path,const void *p,size_t n)
{FILE*f=fopen(path,"wb");int ok;if(!f)return 0;ok=fwrite(p,1u,n,f)==n;fclose(f);return ok;}
int main(void)
{
 const char mot[]="LWMO 1 9 2\n0 0 0 0 0 0 1 1 1 0 1 0 0 0\n1 2 3 0 0 0 1 1 1 1 1 0 0 0\n";
 const char step[]="ISO-10303-21;\nHEADER;\nFILE_DESCRIPTION(('demo'),'2;1');\nFILE_NAME('sample','',(),(),'','','');\nFILE_SCHEMA(('AUTOMOTIVE_DESIGN'));\nENDSEC;\nDATA;\n#1=CARTESIAN_POINT('pt',(0.,1.,2.));\nENDSEC;\nEND-ISO-10303-21;\n";
 const unsigned char scn[]={'S','C','N','\n','1',' ','1',' ','1','\n','1',' ','1',' ','1','\n','8','\n',255};
 const char usd[]="#usda 1.0\n\ndef Mesh \"Triangle\" {\n    point3f[] points = [(0, 0, 0), (1, 0, 0), (0, 1, 0)]\n    int[] faceVertexCounts = [3]\n    int[] faceVertexIndices = [0, 1, 2]\n}\n";
 if(!put("simple.mot",mot,strlen(mot))||!put("simple.step",step,strlen(step))||!put("simple.scn",scn,sizeof(scn))||!put("simple.usda",usd,strlen(usd)))return 1;
 return 0;
}
