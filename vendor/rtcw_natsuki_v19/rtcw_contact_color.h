#ifndef RTCW_CONTACT_COLOR_H
#define RTCW_CONTACT_COLOR_H
/* Contact pages from 640x640 PPM screenshots named pose_###_color.ppm. */
int rtcw_contact_color_generate(const char *folder,unsigned int count);
const char *rtcw_contact_color_error(void);
#endif
