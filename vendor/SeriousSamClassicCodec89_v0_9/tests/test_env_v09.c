#include <stdio.h>
#include "ss89env.h"
static int fail(const char *s){fprintf(stderr,"FAIL: %s\n",s);return 1;}
int main(void)
{
  int a,b,c,d;
  if(!ss89_env_sphere_reflection(0,0,10000,0,0,-10000,&a,&b))return fail("reflection returned false");
  if(a!=5000||b<8400||b>8700)return fail("viewer reflection axis");
  if(!ss89_env_sphere_reflection(10000,0,0,0,0,-10000,&c,&d))return fail("reflection X");
  if(a==c&&b==d)return fail("normal dependent environment mapping");
  if(!ss89_env_sphere_specular(0,0,10000,3350,7890,5100,&a,&b))return fail("spec mapping");
  if(a<0||a>10000||b<0||b>10000)return fail("spec UV range");
  if(!ss89_env_sphere_specular(10000,0,0,3350,7890,5100,&c,&d))return fail("spec mapping X");
  if(a==c&&b==d)return fail("spec orientation normals");
  if(ss89_env_light(0,0,10000,0,0,10000)!=10000)return fail("Lambert front");
  if(ss89_env_light(0,0,-10000,0,0,10000)!=0)return fail("Lambert back");
  if(ss89_env_sphere_reflection(0,0,10000,0,0,-10000,0,&b))return fail("null pointer guard");
  puts("V09 ENV UNIT PASS: reflection plane, specular plane, normal dependence, clamping, Lambert");
  return 0;
}
