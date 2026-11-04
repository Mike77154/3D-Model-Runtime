#include <stdio.h>
#include "ss89settings.h"
int main(int argc,char **argv)
{
    SS89_Settings s;
    if(argc!=2){fprintf(stderr,"usage: ss89settings Model.ini\n");return 2;}
    if(!ss89_settings_read(argv[1],&s)){fprintf(stderr,"settings: invalid WTEX file\n");return 1;}
    printf("specular=%s\nreflection=%s\nbump=%s\n",s.specular,s.reflection,s.bump);
    return 0;
}
