#ifndef SS89_SETTINGS_H
#define SS89_SETTINGS_H
/* Read the optional texture paths of a Serious Engine 1 WTEX model-settings
   resource (.ini). Paths are original resource names, not host file paths. */
typedef struct SS89_Settings {
  char diffuse[260],specular[260],reflection[260],bump[260];
} SS89_Settings;
int ss89_settings_read(const char *file,SS89_Settings *out);
#endif
