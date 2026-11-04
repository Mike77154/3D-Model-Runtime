#include "c4d89.h"
#include <stdio.h>
/* Static callback consumer, representative of streaming bytes to a renderer. */
static unsigned long count;
static int start(void *u,const C4D89_Image *im){(void)u;count=0ul;return im->length>0ul;}
static int chunk(void *u,const unsigned char *p,unsigned int n){
    (void)u;
    if(!p||n==0u)return 0;
    count+=(unsigned long)n;return 1;
}
static int finish(void *u){(void)u;return 1;}
static C4D89_Report report;
int main(int argc,char **argv)
{
    C4D89_ImageSink sink;
    if(argc!=2)return 2;
    sink.begin=start;sink.chunk=chunk;sink.end=finish;
    if(c4d89_scan(argv[1],&report)!=C4D89_OK) return 3;
    if(!report.image_count) return 4;
    if(c4d89_stream_image(argv[1],&report.images[0],&sink,0)!=C4D89_OK)return 5;
    if(count!=report.images[0].length)return 6;
    printf("PASS: streamed %lu bytes\n",count);return 0;
}
